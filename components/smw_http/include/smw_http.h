#ifndef SMW_HTTP_H
#define SMW_HTTP_H

#include "http_client.h"
#include "smw.h"

/**
 * @brief Registers an HTTP request as an SMW worker task.
 * * @param worker Pointer to the SmwWorker structure.
 * @param req Pointer to the HttpClientRequest configuration.
 * @param cb Completion callback triggered when the request is complete.
 * @param user_ctx User-defined context passed to the callback.
 * @param out_task_id Pointer to the task identifier.
 * @return SmwResult SMW_OK on success, or an error code.
 */
SmwResult smw_register_http_request(SmwWorker* worker, const HttpClientRequest* req,
                                    HttpClientDoneCb cb, void* user_ctx, size_t* out_task_id);

#endif // SMW_HTTP_H
