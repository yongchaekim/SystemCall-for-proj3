#include <linux/latencytop.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/profile.h>
#include <linux/interrupt.h>
#include <linux/mempolicy.h>
#include <linux/migrate.h>
#include <linux/task_work.h>

#include <trace/events/sched.h>
#ifdef CONFIG_HMP_VARIABLE_SCALE
#include <linux/sysfs.h>
#include <linux/vmalloc.h>
#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
/* Include cpufreq and ipa headers to add a notifier so that cpu
 * frequency scaling can track the current CPU frequency and limits.
 */
#include <linux/cpufreq.h>
#include <linux/ipa.h>
#endif /* CONFIG_HMP_FREQUENCY_INVARIANT_SCALE */
#endif /* CONFIG_HMP_VARIABLE_SCALE */

//#include <linux/wrr.h>
#include "sched.h"

#define Default_Timeslice 10

/* Structure info the wrr runqueue */
struct wrr_rq {
        int weightsum;  // sum of the weights of all wrr_entity
        int wrr_nr_total; // number of wrr_entity in the list
        struct list_head queue; // queue to store wrr_entity
        int call_load_balance; // tracks the time to call load_balance function
        struct task_struct currenttask; // task_struct of the current runnable task
};

/* struct sched_wrr_entity {
        struct list_head run_list; // list_head for each sched_wrr_entity
        int weight; // weights of the wrr_entity
        int load_on_wrr; // load_on_wrr checks whether the wrr_entity is on the wrr_rq: value 1 states that it is in the runqueue, value 0 state that it is not in the runqueue
        int timeslice; // timeslice of the current runnable task
        struct sched_wrr_entity nexttask; // tracks the next wrr_entity on the wrr_rq, if not then it is null.
}*/

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
        return container_of(wrr_se, struct task_struct, wrr);
}

void init_wrr_rq(struct wrr_rq *wrr_rq)
{
        wrr_rq->weightsum = 0;
        wrr_rq->wrr_nr_total = 0;
        wrr_rq->call_load_balance = 0;
        INIT_LIST_HEAD(&wrr_rq->queue);
}

void enqueue_wrr_task(struct rq *rq, struct task_struct *p)
{
        struct wrr_rq *wrr_rq = &rq->wrr;
        struct sched_wrr_entity *wrr_se = &p->wrr;

        if(wrr_se->load_on_wrr_rq)
                return;

        if(wrr_rq->wrr_nr_total == 0)
        {
                list_add_tail(wrr_se->run_list, wrr_rq->queue);
                wrr_rq->weightsum += wrr_se->weight;
                wrr_rq->wrr_nr_total++;
                wrr_se->load_on_wrr_rq = 1;
                wrr_rq->currenttask = p;

        }
        else
        {
                struct sched_wrr_entity *wrr_se_prev;
                wrr_se_prev = list_last_entry(&wrr_rq->queue, struct sched_wrr_entity, run_list);
                list_add_tail(wrr_se->run_list, wrr_rq->queue);
                wrr_rq->weightsum += wrr_se->weight;
                wrr_rq->wrr_nr_total ++;
                wrr_se->load_on_wrr_rq = 1;
                wrr_se_prev->nexttask = wrr_se;
        }
}

void deqeue_wrr_task(struct rq *rq, struct task_struct *p)
{
        struct wrr_rq *wrr_rq = &rq->wrr;
        struct sched_wrr_entity *wrr_se = &p->wrr;

        if(!wrr_se->load_on_wrr_rq)
                return;

        list_del(wrr_se->run_list);
        wrr_rq->weightsum -= wrr_se->weight;
        wrr_rq->wrr_nr_total--;
        wrr_se->load_on_wrr_rq = 0;

        if(wrr_rq->wrr_nr_total == 0)
                return;
        else
        {
        struct sched_wrr_entity *first_entity;
        first_entity = list_first_entry_or_null(&wrr_rq->queue, struct sched_wrr_entity, run_list);

        if(first_entity == NULL)
                wrr_rq->currenttask = NULL;
        else
                wrr_rq->currenttask = wrr_task_of(first_entity);
        }
}

struct task_struct *pick_next_wrr_task(struct rq *rq)
{
        struct wrr_rq *wrr_rq = &rq->wrr;
        struct task_struct *p = wrr_rq->currenttask;

        if(wrr_rq->wrr_nr_total == 0)
                return NULL;
        if(p == NULL)
                return NULL;
        struct sched_wrr_entity *wrr_se = &p->wrr;
        wrr_se->timeslice = wrr_se->weight * default_Timeslice;
        return p;
}

const struct sched_class wrr_sched_class = {
        .next                           = &fair_sched_class,
        .enqueue_task                   = enqueue_task_wrr,
        .dequeue_task                   = dequeue_task_wrr,
//      .yield_task                     = yield_task_wrr,

//      .check_preempt_curr             = check_preempt_curr_rt,

        .pick_next_task                 = pick_next_task_wrr,
        .put_prev_task                  = put_prev_task_wrr,

#ifdef CONFIG_SMP
        .select_task_rq                 = select_task_rq_wrr,
        .set_cpus_allowed               = set_cpus_allowed_wrr,
        .rq_online                      = rq_online_wrr,
        .rq_offline                     = rq_offline_wrr,
        .pre_schedule                   = pre_schedule_wrr,
        .post_schedule                  = post_schedule_wrr,
        .task_woken                     = task_woken_wrr,
        .switched_from                  = switched_from_wrr,
#endif
        .set_curr_task                  = set_curr_task_wrr,
        .task_tick                      = task_tick_wrr,
        //      .task_fork              = task_fork_fair,

        //.get_rr_interval              = get_rr_interval_wrr,
//      .prio_changed                   = prio_changed_wrr,
        //.switched_from                = switched_from_fair,
        .switched_to                    = switched_to_wrr,
        //.get_rr_interval              = get_rr_interval_fair,
}
