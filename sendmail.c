/*
 * Copyright (c) 2011, Lucas M. de Freitas <lucas@lucasfreitas.eti.br>
 * All rights reserved.
 *
 * Funcao em linguagem C para envio de e-mail no Postgres.
 * 
 * Este programa foi desenvolvido com o intuito de enviar 
 * e-mail atraves da chamada de uma funcao no Postgres.
 * Os metodos para envio de e-mail atraves do servidor SMTP 
 * foram originalmente desenvolvidos por Mayukh Bose 
 * <http://www.mayukhbose.com> e posteriormente modificados 
 * por Lucas M. de Freitas <http://www.lucasfreitas.eti.br> 
 * para suportar o envio de e-mail em HTML e integracao com 
 * chamada de funcao no Postgres.
 * Este codigo pode ser utilizado e modificado para qualquer 
 * fim, desde que se mantenha os direitos autorais, incluindo 
 * os nomes de seus devidos autores.
 * Para entender como este script funciona basta ler o arquivo
 * README.pg_send_mail distribuido junto com este codigo.
 *
 * C-language function for sending email in Postgres.
 *
 * This program was developed in order to send
 * e-mail through the invocation of a function in Postgres.
 * The methods for sending email through SMTP server
 * were originally developed by Bose Mayukh
 * <http://www.mayukhbose.com> and later modified
 * by Lucas M. Freitas <http://www.lucasfreitas.eti.br>
 * support for sending email in HTML and integration with
 * function call in Postgres.
 * This code can be used and modified for any
 * Finally, since it keeps the copyright, including
 * the names of their rightful authors.
 * To understand how this script works just read the file
 * README.pg_send_mail distributed with this code.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of Lucas M. de Freitas and Mayukh Bose nor the names 
 *	  of its contributors may be used to endorse or promote products derived 
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/* 2003-11-17 - Reply-To Mod added by Chris Lacy-Hulbert */
/* 2003-12-26 - Added snprintf fix for WIN32. Thanks Wingman! */
/* 2004-09-09 - Changed the header/body of message to RFC 822 format (section 3.1)
                Thanks to Luke T. Gilbert (Luke.Gilbert@Nav-international.com) for
                the suggestion. */
/* 2011-06-12 - Added support for sending HTML email using 
                Content-Type: text / html by Lucas M. de Freitas <lucas@lucasfreitas.eti.br> */
/* 2011-06-12 - Added support for integration with function call in 
                Postgres by Lucas M. de Freitas <lucas@lucasfreitas.eti.br>. */

#include "postgres.h"
#include "fmgr.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef WIN32
extern "C" {
int send_mail(const char *smtpserver, const char *from, const char *to, 
				const char *subject, const char *replyto, const char *msg);
}
#else
#include "smtp.h"
#endif

#define DEBUG_OUTPUT 1
PG_MODULE_MAGIC;

// Funcao para envio de e-mail
int send_mail(const char *smtpserver, const char *from, const char *to, 
					const char *subject, const char *replyto, const char *msg)
{
	int n_socket;
	int n_retval = 0;

	/* Conecta no STMP via socket */
	/* First connect the socket to the SMTP server */
	if ((n_socket = connect_to_server(smtpserver)) == ERROR) 
		n_retval = E_NO_SOCKET_CONN;

	/* Inicia a sessao com o servidor SMTP */
	/* All connected. Now send the relevant commands to initiate a mail transfer */
	if (n_retval == 0 && send_command(n_socket, "MAIL From:<", from, ">\r\n", MAIL_OK) == ERROR)
		n_retval = E_PROTOCOL_ERROR;
	if (n_retval == 0 && send_command(n_socket, "RCPT To:<", to, ">\r\n", MAIL_OK) == ERROR) 
		n_retval = E_PROTOCOL_ERROR;

	/* Envia a mensagem */
	/* Now send the actual message */
	if (n_retval == 0 && send_command(n_socket, "", "DATA", "\r\n", MAIL_GO_AHEAD) == ERROR) 
		n_retval = E_PROTOCOL_ERROR;
	if (n_retval == 0 && send_mail_message(n_socket, from, to, subject, replyto, msg) == ERROR) 
		n_retval = E_PROTOCOL_ERROR;

	/* Finaliza a sessao com o servidor SMTP */
	/* Now tell the mail server that we're done */
	if (n_retval == 0 && send_command(n_socket, "", "QUIT", "\r\n", MAIL_GOODBYE) == ERROR) 
		n_retval = E_PROTOCOL_ERROR;

	/* Finaliza a conexao */
	/* Now close up the socket and clean up */
	if (close(n_socket) == ERROR) {
		elog(NOTICE, "Erro ao fechar o socket.");
		//elog(NOTICE, "Could not close socket.\n");
		n_retval = ERROR;
	}

	return n_retval;
}

int connect_to_server(const char *server)
{
	struct hostent *host;
	struct in_addr	inp;
	struct protoent *proto;
	struct sockaddr_in sa;
	int n_sock;
    #define SMTP_PORT	   25 // porta do servidor SMTP / SMTP port
    #define BUFSIZE		4096 
	char s_buf[BUFSIZE] = "";
	int n_ret;

	/* Resolve o hostname do SMTP */
	/* First resolve the hostname */
	host = gethostbyname(server);
	if (host == NULL) {
		elog(NOTICE, "Erro ao resolver o hostname %s do servidor de e-mail. Abortando...", server);
		//elog(NOTICE, "Could not resolve hostname %s. Aborting...\n", server);
		return ERROR;
	}

	memcpy(&inp, host->h_addr_list[0], host->h_length);

	/* Obtendo acesso ao protocolo TCP */
	/* Next get the entry for TCP protocol */
	if ((proto = getprotobyname("tcp")) == NULL) {
		elog(NOTICE, "Erro ao solicitar protocolo TCP. Abortando...");
		//elog(NOTICE, "Could not get the protocol for TCP. Aborting...\n");
		return ERROR;	
	}

	/* Criando estrutura do socket */
	/* Now create the socket structure */
	if ((n_sock = socket(PF_INET, SOCK_STREAM, proto->p_proto)) == INVALID_SOCKET) {
		elog(NOTICE, "Erro ao criar o socket TCP. Abortando...");
		//elog(NOTICE, "Could not create a TCP socket. Aborting...\n");
		return ERROR;
	}

	/* Conectando ao socket do servidor SMTP */
	/* Now connect the socket */
	memset(&sa, 0, sizeof(sa));
	sa.sin_addr = inp;
	sa.sin_family = host->h_addrtype;
	sa.sin_port = htons(SMTP_PORT);
	if (connect(n_sock, (struct sockaddr *)&sa, sizeof(sa)) == SOCKET_ERROR) {
		elog(NOTICE, "Conexao recusada pelo servidor de e-mail %s.", server);
		//elog(NOTICE, "Connection refused by host %s.", server);
		return ERROR;
	}

	/* Lendo mensagem servidor SMTP */
	/* Now read the welcome message */
	n_ret = recv(n_sock, s_buf, BUFSIZE, 0);
	
	return n_sock;
}

int send_command(int n_sock, const char *prefix, const char *cmd, 
						const char *suffix, int ret_code)
{
    #define BUFSIZE		4096
	char s_buf[BUFSIZE] = "";
	char s_buf2[50];
	
 	strncpy(s_buf, prefix, BUFSIZE);
	strncat(s_buf, cmd, BUFSIZE);
	strncat(s_buf, suffix, BUFSIZE);

	if (send(n_sock, s_buf, strlen(s_buf), 0) == SOCKET_ERROR) {
		elog(NOTICE, "Erro ao enviar o comando %s para o servidor de e-mail.", s_buf);
		//elog(NOTICE, "Could not send command string %s to server.", s_buf);
		return ERROR;
	}

	/* Lendo a resposta do servidor SMTP */
	/* Now read the response. */
	recv(n_sock, s_buf, BUFSIZE, 0);

	/* Verifica se o ret_code esta no buf */
	/* Now check if the ret_code is in the buf */
	sprintf(s_buf2, "%d", ret_code);

	if (strstr(s_buf, s_buf2) != NULL)
		return TRUE;
	else
		return ERROR;
}

int send_mail_message(int n_sock, const char *from, const char *to,
							const char *subject, const char *replyto, const char *msg)
{
    #define BUFSIZE		4096
    #define BUFSIZE2	100
    #define MSG_TERM	"\r\n.\r\n"
    #define MAIL_AGENT	"PG_SENDMAIL FOR POSTGRESQL by Lucas M. de Freitas <lucas@lucasfreitas.eti.br>"
    #define CONTENT_TYPE "text/html" // CONTENT_TYPE
	char s_buf[BUFSIZE];
	char s_buf2[BUFSIZE2];
	time_t t_now = time(NULL);
	int n_ret;

	/* Preparando o envelope do e-mail */
	/* First prepare the envelope */
	strftime(s_buf2, BUFSIZE2, "%a, %d %b %Y  %H:%M:%S +0000", gmtime(&t_now));

	snprintf(s_buf, BUFSIZE, "Date: %s\r\nFrom: %s\r\nTo: %s\r\nSubject: %s\r\nX-Mailer: %s\r\nReply-To: %s\r\nContent-Type: %s\r\n\r\n",
				s_buf2, from, to, subject, MAIL_AGENT, replyto, CONTENT_TYPE); 

	/* Enviando o envelope */
	/* Send the envelope */
	if (send(n_sock, s_buf, strlen(s_buf), 0) == SOCKET_ERROR) {
		elog(NOTICE, "Erro ao enviar o cabecalho do e-mail: %s.", s_buf);
		//elog(NOTICE, "Could not send message header: %s", s_buf);
		return ERROR;
	}

	/* Enviando a mensagem */
	/* Now send the message */
	if (send(n_sock, msg, strlen(msg), 0) == SOCKET_ERROR) {
		elog(NOTICE, "Erro ao enviar a mensagem: %s\n", msg);
		//elog(NOTICE, "Could not send the message %s\n", msg);
		return ERROR;
	}

	/* Enviando sinal de desfecho */
	/* Now send the terminator*/
	if (send(n_sock, MSG_TERM, strlen(MSG_TERM), 0) == SOCKET_ERROR) {
		elog(NOTICE, "Erro ao enviar a mensagem de desfecho: %s\n", msg);
		//elog(NOTICE, "Could not send the message terminator: %s\n", msg);
		return ERROR;
	}

	/* Lendo e descartando a mensagem de retorno */
	/* Read and discard the returned message ID */
	n_ret = recv(n_sock, s_buf, BUFSIZE, 0);

	return TRUE;
}

// Converte text para char
// Converts text to char
static char * text2char(text *in)
{
    char *out = palloc(VARSIZE(in));
    memcpy(out, VARDATA(in), VARSIZE(in) - VARHDRSZ);
    out[VARSIZE(in) - VARHDRSZ] = '\0';
    return out;
} 

// Funcao que sera acessada pelo Postgres
// Function that will be accessed by Postgres
extern Datum sendmail( PG_FUNCTION_ARGS );

PG_FUNCTION_INFO_V1( sendmail );
Datum sendmail( PG_FUNCTION_ARGS )
{
             
    char *host_mta; // host do servidor smtp / smtp host
    char *email_origem; // email de origem / source mail
    char *email_destino; // email de destino / destination mail
    char *email_resposta; // email para resposta / response mail
    char *assunto; // assunto do email / subject
    char *mensagem; // mensagem do email / message
             
    // Verifica se argumentos foram recebidos
	// Checks if the arguments were received
    if( PG_ARGISNULL(0) || PG_ARGISNULL(1) || PG_ARGISNULL(2) || PG_ARGISNULL(3) || PG_ARGISNULL(4) || PG_ARGISNULL(5)  ) {
        PG_RETURN_NULL();
    }
    
	// Recebendo parametros passados para a funcao do postgres
	// Getting parameters passed to the function in postgres
    host_mta = text2char(PG_GETARG_TEXT_P(0));
    email_origem = text2char(PG_GETARG_TEXT_P(1));
    email_destino = text2char(PG_GETARG_TEXT_P(2));
    email_resposta = text2char(PG_GETARG_TEXT_P(3));
    assunto = text2char(PG_GETARG_TEXT_P(4));
    mensagem = text2char(PG_GETARG_TEXT_P(5));

	// enviando o e-mail
	// sending the mail
    if (send_mail(host_mta, email_origem, email_destino, assunto, email_resposta, mensagem) == 0) {
        elog(NOTICE, "E-mail enviado com sucesso!");
		//elog(NOTICE, "Email sent successfully!");
        PG_RETURN_BOOL(true);
	} else {
        elog(NOTICE, "Erro ao enviar o e-mail! Verifique os parametros e tente novamente.");
		//elog(NOTICE, "Error sending email! Check the parameters and try again.");
        PG_RETURN_BOOL(false); 
    }

}

