#include "common/common.h"
#include "common/posix_utils.h"
#include "common/caesar_cipher.h"
#include "client_utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

void explainAndDie(char **argv) {
    printf("\nInvalid Input\n");
    printf("Usage: %s <server IP> <server port> <text> <encryption key number>\n", argv[0]);
    printf("Only lowercase with no spaces strings are accepted as text!\n");
	printf("Example: %s 127.0.0.1 5000 lorenipsumdolur 4\n", argv[0]);
    exit(EXIT_FAILURE);
}

/**
 * ------------------------------------------------
 * == Programa CLIENTE ============================
 * ------------------------------------------------
 * 
 * TODO: 2021-05-27 - ADD Descricao
 * TODO: 2021-05-27 - Resolver todo's
 * 
 */
int main(int argc, char **argv) {

	const int dbgTxtLen = DEBUG_ENABLE ? 200 : 0;
	char dbgTxt[dbgTxtLen];
	commonDebugStep("\nStarting...\n\n");

    /*=================================================== */
    /*-- Validar entrada -------------------------------- */

	commonDebugStep("Validating input...\n");
    if (!clientValidateInput(argc, argv)) {
        explainAndDie(argv);
    }

	/*
		Define endereco do socket:

		- Struct sockaddr_storage equivale a uma 'super classe';
		- Permite alocar enderecos tanto ipv4 quanto ipv6;
		- sockaddr_in / sockaddr_in6;
	*/

	commonDebugStep("Parsing addr...\n");
	struct sockaddr_storage addr;
	const char *addrStr = argv[1];
	const char *portStr = argv[2];
	if (!clientParseAddress(addrStr, portStr, &addr)) // Funcao customizada
		explainAndDie(argv);
    
	/*=================================================== */
    /*-- Conectar com servidor -------------------------- */

	commonDebugStep("Creating socket...\n");
	int socketFD = socket(addr.ss_family, SOCK_STREAM, 0); // socket tcp (existem outros tipos)
	if (socketFD == -1)
		commonLogErrorAndDie("Failure as creating socket [1]");

	// Define timeout de escuta
    commonDebugStep("Setting listening timeout...\n");

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECS;
    timeout.tv_usec = 0;

    if (setsockopt(socketFD, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
		commonLogErrorAndDie("Failure as creating socket [2]");

	/*
		Cria conexao no enderenco (IP + Porta) do socket
		- Struct sockaddr equivale a uma 'interface' implementada por sockaddr_in / sockaddr_in6;
	*/

	commonDebugStep("Creating connection...\n");

	if (connect(socketFD, (struct sockaddr *)(&addr), sizeof(addr)) != 0)
		commonLogErrorAndDie("Failure as connecting to server");
	
	if (DEBUG_ENABLE) {
		sprintf(dbgTxt, "\nConnected to %s:%s\n", addrStr, portStr);
		commonDebugStep(dbgTxt);
	}

	/*=================================================== */
    /*-- Enviar tamanho da string ----------------------- */

	char buffer[BUF_SIZE];
	
	commonDebugStep("Sending message length...\n");
	const char *text = argv[3];
	uint32_t txtLen = htonl(strlen(text));
	clientSendParam(socketFD, buffer, txtLen, &timeout, CLI_SEND_PARAM_NUM, 1, 1);

	/*=================================================== */
    /*-- Enviar chave da cifra -------------------------- */

	commonDebugStep("Sending encryption key...\n");
	const char *cipherKeyStr = argv[4];
	uint32_t cipherKey = htonl(atoi(cipherKeyStr));
	clientSendParam(socketFD, buffer, &cipherKey, &timeout, CLI_SEND_PARAM_NUM, 2, 1);

	/*=================================================== */
    /*-- Enviar string cifrada -------------------------- */

	commonDebugStep("Sending message...\n");
	memset(buffer, 0, BUF_SIZE); // Inicializar buffer com 0
	caesarCipher(text, txtLen, buffer, cipherKey);
	clientSendParam(socketFD, buffer, &cipherKey, &timeout, CLI_SEND_PARAM_STR, 2, 0);

	/*=================================================== */
    /*-- Receber resposta (string desencriptografada) --- */

	commonDebugStep("Waiting server answer...\n");
	
	memset(buffer, 0, BUF_SIZE);
	unsigned receivedBytes = 0;
	posixReceive(socketFD, buffer, &receivedBytes, &timeout);

	if (receivedBytes < txtLen) {
		sprintf(dbgTxt, "Invalid deciphered response from server: \"%.1000s\"\n", buffer);
		commonLogErrorAndDie(dbgTxt);
	}

	if (DEBUG_ENABLE) {
		sprintf(dbgTxt, "\tReceived %u bytes!\n", receivedBytes);
		commonDebugStep(dbgTxt);
	}

	/*=================================================== */
    /*-- Imprimir resposta ------------------------------ */
	
	puts(buffer);
	close(socketFD);
	exit(EXIT_SUCCESS);
}
