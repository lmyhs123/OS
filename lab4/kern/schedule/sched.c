#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

//LAB4 function3 2413715
// 从当前线程后方循环寻找可运行线程，没有候选时退回空闲线程。
void schedule(void) {
    bool intr_flag;
    local_intr_save(intr_flag);
    current->need_resched = 0;
    list_entry_t *head = &proc_list;
    list_entry_t *start = current == idleproc ? head : &current->list_link;
    list_entry_t *le = start;
    struct proc_struct *next = idleproc;
    do {
        le = list_next(le);
        if (le != head) {
            struct proc_struct *p = le2proc(le, list_link);
            if (p->state == PROC_RUNNABLE) { next = p; break; }
        }
    } while (le != start);
    next->runs++;
    proc_run(next);
    local_intr_restore(intr_flag);
}
