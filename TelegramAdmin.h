#ifndef TELEGRAM_ADMIN_H
#define TELEGRAM_ADMIN_H

#include "Config.h"

// ============================================================================
// ATALAIA 3 — GERENCIAMENTO ADMINISTRATIVO E SEGURANÇA DO TELEGRAM
// Protocolo: ASD-STE100 (PT-BR) | Operação TLS 1.2 Não-Bloqueante
// ============================================================================

/**
 * @brief Remove qualquer webhook ativo na API do Telegram.
 * Libera o endpoint getUpdates contra erros HTTP 409 Conflict.
 * @param dropUpdates Se verdadeiro, descarta mensagens pendentes acumuladas.
 * @return true em caso de sucesso da requisição, false em falha de conexão.
 */
bool resetTelegramWebhook(bool dropUpdates = false);

/**
 * @brief Testa a conectividade do bot com o endpoint oficial /getMe.
 * Valida se o token configurado é autêntico e recupera dados da conta.
 * @return true se o bot responder com HTTP 200 OK e JSON válido.
 */
bool testTelegramApiConnection();

/**
 * @brief Executa a purga de mensagens antigas retidas na fila da nuvem.
 * @return true se a fila for limpa com sucesso.
 */
bool purgeTelegramPendingUpdates();

/**
 * @brief Valida se o Chat ID remetente possui autorização de comando.
 * @param chatId Identificador numérico único do usuário no Telegram.
 * @return true se o chatId for estritamente igual a AUTHORIZED_CHAT_ID.
 */
bool validateSenderAuthorization(int64_t chatId);

#endif // TELEGRAM_ADMIN_H