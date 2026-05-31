/**
 * @file console_cli.h
 * @brief Serial Command Line Interface (CLI) component using ESP-IDF Console.
 */

#ifndef CONSOLE_CLI_H
#define CONSOLE_CLI_H

#ifdef __cplusplus
extern "extern C" {
#endif

/**
 * @brief Initializes the console hardware, registers custom commands,
 * and spawns the background FreeRTOS CLI handler task.
 * * @note This should be called after your global application structures
 */
void console_cli_start(void);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_CLI_H
