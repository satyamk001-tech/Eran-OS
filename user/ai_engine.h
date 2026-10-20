#ifndef ERAN_USER_AI_ENGINE_H
#define ERAN_USER_AI_ENGINE_H

#include <stdint.h>
#include <stddef.h>

void ai_engine_init(void);
void ai_process_query(const char *natural_language_query);

#endif /* ERAN_USER_AI_ENGINE_H */
