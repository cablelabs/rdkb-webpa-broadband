/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */
#include "webpa_rpc.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <rpc/pmap_clnt.h>
#include <time.h>
#include <unistd.h>
#include "wal.h"

#define MAX_CLIENTS 8

typedef void (*WebPA_ClientConnector_Dispatcher)(int n, char const* buff);
typedef struct _message_queue
{
  char*                  buff;
  int                    n;
  struct _message_queue* next;
} message_queue;

void webpa_prog_1(struct svc_req *rqstp, register SVCXPRT *transp);


int WebPA_ClientConnector_DispatchMessage(char const* topic, char const* buff, int n);
int WebPA_ClientConnector_Start();
int WebPA_ClientConnector_SetDispatchCallback(WebPA_ClientConnector_Dispatcher callback);

typedef struct
{
  char*             topic;
  int               prog_num;
  int               prog_vers;
  char*             proto;
  char*             host;
  CLIENT*           clnt;
  int               keep_alive_interval;
  pthread_mutex_t   mutex;
  pthread_cond_t    cond;
  message_queue*    queue;
} WebPA_Client;


static WebPA_Client* clients[MAX_CLIENTS];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int WebPA_Client_IsMatch(WebPA_Client* c, char const* topic);
static enum clnt_stat WebPA_Client_SendMessage(WebPA_Client* c, char const* buff, int n);
static void WebPA_Client_EnqueueMessage(WebPA_Client* c, char const* buff, int n);

static pthread_t server_thread;
static WebPA_ClientConnector_Dispatcher message_dispatch_callback = NULL;

int WebPA_ClientConnector_SetDispatchCallback(WebPA_ClientConnector_Dispatcher callback)
{
  pthread_mutex_lock(&mutex);
  message_dispatch_callback = callback;
  pthread_mutex_unlock(&mutex);
  WalInfo("WebPA_ClientConnector_SetDispatchCallback: Successfully set the callback function\n");
}

static void WebPA_Server_ClearPendingSignal(int signum)
{
  sigset_t s;
  sigemptyset(&s);
  sigpending(&s);
  if (sigismember(&s, signum))
  {
    struct timespec zerotime;
    zerotime.tv_sec = 0;
    zerotime.tv_nsec = 0;

    sigemptyset(&s);
    sigaddset(&s, signum);
    sigtimedwait(&s, 0, &zerotime);
  }
}

//getsockfd
static int getSocketFd()
{
  int sockfd;
  struct sockaddr_in servAddr;
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0)
  {
    WalError("Error sockfd\n");
    exit(1);
  }
  bzero((char *)&servAddr, sizeof(servAddr));
  
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = inet_addr("192.168.254.253");//TODO get from interface
  servAddr.sin_port = 0;
  
  if(bind(sockfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0 )
  {
    WalError("Error sockfd bind\n");
    exit(1);    
  }
  return sockfd;
}

static void* WebPA_Server_Run(void* argp)
{
  SVCXPRT*  xprt;

  (void) argp;

  int sock = getSocketFd();


  WalInfo("Entering WebPA_Server_Run thread\n");
  pmap_unset(WEBPA_PROG, WEBPA_VERS);
  //xprt = svctcp_create(RPC_ANYSOCK, 0, 0); // to create (bind) socket first
  xprt = svctcp_create(sock, 0, 0); // to create (bind) socket first
  if (!xprt)
  {
    WalError("can't create service\n");
    pthread_exit(1);
    // TODO
  }

  if (!svc_register(xprt, WEBPA_PROG, WEBPA_VERS, webpa_prog_1, IPPROTO_TCP))
  {
    fprintf (stderr, "%s", "unable to register (WEBPA_PROG, WEBPA_VERS, tcp).");
    pthread_exit(1);
    // TODO:
  }

  svc_run();
  return NULL;
}

int WebPA_ClientConnector_Start()
{
  int err = 0;

  err = pthread_create(&server_thread, NULL, WebPA_Server_Run, NULL);
  if (err != 0) 
  {
    WalError("Error creating WebPA_Server_Run thread :[%s]\n", strerror(err));
  }
  else
  {
    WalInfo("WebPA_Server_Run thread created Successfully\n");
  }

  return 0;
}


int WebPA_ClientConnector_DispatchMessage(char const* topic, char const* buff, int n)
{
  int i;

  pthread_mutex_lock(&mutex);
  for (i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients[i] && WebPA_Client_IsMatch(clients[i], topic))
    {
      WalInfo("Match found clients[%d]->topic %s \n", i,clients[i]->topic);
      //WebPA_Client_SendMessage(clients[i], buff, n);
      WebPA_Client_EnqueueMessage(clients[i], buff, n);
    }
  }
  pthread_mutex_unlock(&mutex);
  return 0;
}

static int WebPA_Client_IsMatch(WebPA_Client* c, char const* topic)
{
  if (!c) return 0;
  if (!topic) return 0;
  return strcmp(c->topic, topic) == 0;
}

void WebPA_Client_EnqueueMessage(WebPA_Client* c, char const* buff, int n)
{
  message_queue* p;
  message_queue* item;
  
  item = (message_queue *) malloc(sizeof(message_queue));
  item->n = n;
  item->buff = (char *) malloc(n);
  item->next = NULL;
  memcpy(item->buff, buff, n);

  pthread_mutex_lock(&c->mutex);
  if (!c->queue)
  {
    c->queue = item;
  }
  else
  {
    p = c->queue;
    while (p->next)
        p = p->next;
    p->next = item;
  }
  pthread_mutex_unlock(&c->mutex);
  pthread_cond_signal(&c->cond);
}

static enum clnt_stat WebPA_Client_SendMessage(WebPA_Client* c, char const* buff, int n)
{
  enum clnt_stat              st;
  webpa_send_message_request  req;
  webpa_send_message_response res;
  struct timeval              timeout;
  
  res.ack = 0;
  req.data.data_len = n;
  req.data.data_val = (char *) malloc(n);
  memcpy(req.data.data_val, buff, n);
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;

  st = RPC_CANTSEND;

  if (c->clnt)
  {
    // block SIGPIPE for this call
    sigset_t mask;
    sigset_t old_mask;
    sigemptyset(&mask);
    sigemptyset(&old_mask);
    sigaddset(&mask, SIGPIPE);
    
    pthread_sigmask(SIG_BLOCK, &mask, &old_mask);
    
    WalInfo("Sending message to client: %s len: %d data: %s\n", c->topic, req.data.data_len, req.data.data_val);
    st = clnt_call(c->clnt, WEBPA_SEND_MESSAGE, (xdrproc_t) xdr_webpa_send_message_request,
        (caddr_t) &req, (xdrproc_t) xdr_webpa_send_message_response, (caddr_t) &res, timeout);
    
    // clear SIGPIPE before unblocking
    if (st != RPC_SUCCESS)
      WebPA_Server_ClearPendingSignal(SIGPIPE);

    pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
  }

 
  if (st != RPC_SUCCESS)
  {
    if (c->clnt)
    {
      char const* s = clnt_sperror(c->clnt, "failed to send message");
      WalError("%s\n", s);
    }
  }
  
  return st;
}

static void WebPA_Client_Destroy(WebPA_Client* c)
{
  if (!c)         return;
  if (c->topic)   free(c->topic);
  if (c->proto)   free(c->proto);
  if (c->host)    free(c->host);
  if (c->clnt)    clnt_destroy(c->clnt);
  pthread_mutex_destroy(&c->mutex);
  free(c);
}

static WebPA_Client* WebPA_Client_Create(webpa_register_request* req)
{
  WebPA_Client* c = (WebPA_Client *) malloc(sizeof(WebPA_Client));
  if (c)
  {
    pthread_mutex_init(&c->mutex, NULL);

    if (req->topics.topics_len > 0)
      c->topic = strdup(req->topics.topics_val[0].topic);

    c->prog_num = req->prog_num;
    c->prog_vers = req->prog_vers;

    if (req->proto)
      c->proto = strdup(req->proto);

    if (req->host)
      c->host= strdup(req->host);

    c->clnt = NULL;
    c->keep_alive_interval = 1;
  }
  return c;
}

static void* WebPA_ServiceClient(void* argp)
{
  int                       i;
  int                       n;
  WebPA_Client*             c;
  enum clnt_stat            st;
  struct timespec           ts;

  // prevent SIGPIPE when client disconnects
  {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
  }

  // This is the client that just connected to us. we use this to send messages
  // to the client, and to invoke the NULLPROC (ping) to make sure it's still
  // connected
  c = (WebPA_Client *) argp;

  while (1)
  {
    pthread_mutex_lock(&c->mutex);
    if (!c->clnt)
      c->clnt = clnt_create(c->host, c->prog_num, c->prog_vers, c->proto);

    if (!c->clnt)
    {
      char const* s = clnt_spcreateerror(c->host);
      WalError("clnt_create failed: %s\n", s);
      pthread_mutex_unlock(&c->mutex);
      break;
    }
    else
    {
      struct timeval timeout;
      timeout.tv_sec = 2;
      timeout.tv_usec = 0;

      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += 5;

      n = pthread_cond_timedwait(&c->cond, &c->mutex, &ts);
      if (n == ETIMEDOUT)
      {
          st = clnt_call(c->clnt, NULLPROC, (xdrproc_t) xdr_void, (caddr_t) NULL,
                  (xdrproc_t) xdr_void, (caddr_t) NULL, timeout);
      }

      if (c->queue)
      {
        message_queue* p;
        message_queue* q = c->queue;
        while (q)
        {
          st = WebPA_Client_SendMessage(c, q->buff, q->n);
          p = q;
          q = q->next;
        
          free(p->buff);
          free(p);
        }
        c->queue = NULL;
      }

      if (st != RPC_SUCCESS)
      {
        WebPA_Server_ClearPendingSignal(SIGPIPE);

        char const* s = clnt_sperrno(st);
        WalError("clnt_call failed: %s\n", s);
        pthread_mutex_unlock(&c->mutex);       
        break;
      }
    }
    pthread_mutex_unlock(&c->mutex);
  }

  pthread_mutex_lock(&mutex);
  for (i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients[i] == c)
      clients[i] = NULL;
  }
  pthread_mutex_unlock(&mutex);

  WebPA_Client_Destroy(c);

  return NULL;
}

static void InitClientList()
{
  int i;
  for (i = 0; i < MAX_CLIENTS; ++i)
    clients[i] = NULL;
}

// client asked webpa server to send message
bool_t
webpa_send_message_1_svc(webpa_send_message_request req, webpa_send_message_response* res, struct svc_req* svc)
{
  (void) svc;

  if (message_dispatch_callback)
    message_dispatch_callback(req.data.data_len, req.data.data_val);

  return TRUE;
}

// client is registering it's embedded rpc server with webpa
bool_t
webpa_register_1_svc(webpa_register_request req, webpa_register_response* res, struct svc_req* svc)
{
  int             i;
  int             index;
  pthread_t       thr;
  WebPA_Client*   client = NULL;

  (void) svc;

  pthread_mutex_lock(&mutex);

  // first check to see if client is already registered
  for (i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients[i])
    {
      if ((clients[i]->prog_num == req.prog_num) &&
          (clients[i]->prog_vers == req.prog_vers) &&
          (strcmp(clients[i]->proto, req.proto) == 0))
      {
        // TODO: already registered???
        WalInfo("Client already registered\n");
      }
    }
  }

  for (i = 0, index = -1; index == -1 && i < MAX_CLIENTS; ++i)
  {
    if (!clients[i])
      index = i;
  }

  if (index != -1)
    client = WebPA_Client_Create(&req);
  
  if(client)
    WalInfo("client created with topic: %s\n",client->topic);
  else
    WalError("client not created \n");
  
  clients[index] = client;
  pthread_mutex_unlock(&mutex);

  if (index == -1)
    return FALSE;

  if (!client)
    return FALSE;

  pthread_create(&thr, NULL, WebPA_ServiceClient, client);
  res->id = index;
  return TRUE;
}

int
webpa_prog_1_freeresult (SVCXPRT* transp, xdrproc_t xdr_result, caddr_t result)
{
  (void) transp;

	xdr_free (xdr_result, result);

	/*
	 * Insert additional freeing code here, if needed
	 */
	return 1;
}
