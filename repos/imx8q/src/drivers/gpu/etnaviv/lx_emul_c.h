/**
 * \brief  Backend implementation for Linux
 * \author Josef Soentgen
 * \date   2021-03-25
 */

#ifdef __cplusplus
extern "C" {
#endif

void genode_emul_update_expires_timer(void *, unsigned long);
void genode_emul_execute_timer(void *);
int  genode_emul_interrupt_handler(void *, unsigned int, int (*handler)(int, void*), int (*thread_fn)(int, void*));

void genode_emul_execute_work(void *);

#ifdef __cplusplus
}
#endif
