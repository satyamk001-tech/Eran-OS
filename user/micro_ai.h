#ifndef ERAN_USER_MICRO_AI_H
#define ERAN_USER_MICRO_AI_H

#include <stdint.h>
#include <stddef.h>

#define NEURAL_INPUT_DIM   32
#define NEURAL_HIDDEN_DIM  16
#define NEURAL_OUTPUT_DIM  8

typedef enum {
    INTENT_SYSTEM_TELEMETRY = 0,
    INTENT_MEMORY_INSPECTION = 1,
    INTENT_STORAGE_FILESYSTEM = 2,
    INTENT_NETWORK_DIAGNOSTICS = 3,
    INTENT_AUDIO_SOUND = 4,
    INTENT_GUI_DESKTOP = 5,
    INTENT_SECURITY_PRIVILEGE = 6,
    INTENT_GENERAL_HELP = 7
} neural_intent_t;

typedef struct {
    neural_intent_t predicted_intent;
    const char *intent_name;
    int confidence_percent;
    int logits[NEURAL_OUTPUT_DIM];
} neural_result_t;

void micro_ai_init(void);
neural_result_t micro_ai_infer(const char *prompt);
void micro_ai_process_query(const char *prompt);
void micro_ai_print_inspection(const char *prompt);

#endif /* ERAN_USER_MICRO_AI_H */
