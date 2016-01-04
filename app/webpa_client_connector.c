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
#include <arpa/inet.h>
#include "wal.h"

#define MAX_CLIENTS 8

typedef void (*WebPA_ClientConnector_Dispatcher)(int n, char const* buff);
typedef struct _message_queue
{
  char*                  buff;
  int                    n;
  struct _message_queue* next;
} message_queue;

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
  int               is_dead;
  pthread_t         worker_thread;
} WebPA_Client;

void webpa_prog_1(struct svc_req *rqstp, register SVCXPRT *transp);
WAL_STATUS WebPA_ClientConnector_DispatchMessage(char const* topic, char const* buff, int n);
int WebPA_ClientConnector_Start();
int WebPA_ClientConnector_SetDispatchCallback(WebPA_ClientConnector_Dispatcher callback);

WAL_STATUS msgStatus = WAL_FAILURE;
pthread_mutex_t msgStatusMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t msgStatusCond = PTHREAD_COND_INITIALIZER;

static WebPA_Client* clients[MAX_CLIENTS];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int WebPA_Client_IsMatch(WebPA_Client* c, char const* topic);
static enum clnt_stat WebPA_Client_SendMessage(WebPA_Client* c, char const* buff, int n);
static WAL_STATUS WebPA_Client_EnqueueMessage(WebPA_Client* c, char const* buff, int n);

static pthread_t server_thread;
static WebPA_ClientConnector_Dispatcher message_dispatch_callback = NULL;

__attribute__((weak))
in_addr_t get_npcpu_from_config()
{
  char  buff[1024];

  FILE* f = fopen("/etc/config", "r");
  if (!f)
    return inet_addr("192.168.254.253");

  while (fgets(buff, sizeof(buff), f) != NULL)
  {
    if (strncmp(buff, "CONFIG_SYSTEM_RPC_IF_NPCPU_ADDR", 31) == 0)
    {
      char* begin;
      char* end;

      begin = buff;
      while (begin && *begin != '"')
        begin++;

      if (begin) begin++;

      end = begin;
      while (end && *end != '"')
        end++;

      if (end) *end = '\0';

      fclose(f);
      return inet_addr(begin);
    }
  }

  fclose(f);
  return inet_addr("192.168.254.253");
}

int WebPA_ClientConnector_SetDispatchCallback(WebPA_ClientConnector_Dispatcher callback)
{
  pthread_mutex_lock(&mutex);
  message_dispatch_callback = callback;
  pthread_mutex_unlock(&mutex);
  WalInfo("WebPA_ClientConnector_SetDispatchCallback: Successfully set the callback function\n");
  return 0;
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
    int err = errno;
    WalError("Error sockfd socket creation failed\n");
    return err;
  }
  
  bzero((char *)&servAddr, sizeof(servAddr));
  
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = get_npcpu_from_config();
  servAddr.sin_port = 0;
  
  if (bind(sockfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0 )
  {
    int err = errno;
    WalError("Error sockfd bind\n");
    return err;
  }
  return sockfd;
}

static void* WebPA_Server_Run(void* argp)
{
  SVCXPRT*  xprt = NULL;
  bool_t ret = 0;

  (void) argp;

  int sock = getSocketFd();

  WalInfo("Entering WebPA_Server_Run thread\n");
  ret = pmap_unset(WEBPA_PROG, WEBPA_VERS);
  
  if(ret != 1)
  {
    WalError("pmap_unset failed\n"); 
    //TODO any action required
  }
  
  //xprt = svctcp_create(RPC_ANYSOCK, 0, 0); // to create (bind) socket first
  xprt = svctcp_create(sock, 0, 0); // to create (bind) socket first
  if (xprt == NULL)
  {
    int err = errno;
    WalError("can't create service\n");
    pthread_exit(&err);
  }
  
  ret = svc_register(xprt, WEBPA_PROG, WEBPA_VERS, webpa_prog_1, IPPROTO_TCP);
  if (ret == 0)
  {
    int err = errno;
    WalError("unable to register rpc program (prog=%d, vers=%d, proto=tcp)\n", WEBPA_PROG, WEBPA_VERS);
    pthread_exit(&err);
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


WAL_STATUS WebPA_ClientConnector_DispatchMessage(char const* topic, char const* buff, int n)
{
  int i;
  WAL_STATUS ret = WAL_FAILURE;

  pthread_mutex_lock(&mutex);
  
  for (i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients[i] && WebPA_Client_IsMatch(clients[i], topic))
    {
      WalInfo("Match found clients[%d]->topic %s \n", i, clients[i]->topic);
      ret = WebPA_Client_EnqueueMessage(clients[i], buff, n);      
    }
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

static int WebPA_Client_IsMatch(WebPA_Client* c, char const* topic)
{
  if (!c) return 0;
  if (!topic) return 0;
  return strcmp(c->topic, topic) == 0;
}

WAL_STATUS WebPA_Client_EnqueueMessage(WebPA_Client* c, char const* buff, int n)
{
  message_queue* p = NULL;
  message_queue* item = NULL;
  
  item = (message_queue *) malloc(sizeof(message_queue));
  if(item == NULL)
  {
    WalError("item Malloc failed");
    return WAL_FAILURE;
  }
  
  item->n = n;
  item->buff = (char *) malloc(n);
  if(item->buff == NULL)
  {
    WalError("item->buff Malloc failed");
    free(item);
    return WAL_FAILURE;
  }
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
  pthread_cond_signal(&c->cond);
  pthread_mutex_unlock(&c->mutex);
  
  return WAL_SUCCESS;
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
  if (req.data.data_val == NULL)
  {
    WalError("req.data.data_val malloc failed");
    return RPC_FAILED;
  }
  
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
    
    WalInfo("Sending message to client: %s\n", c->topic);
    st = clnt_call(c->clnt, WEBPA_SEND_MESSAGE, (xdrproc_t) xdr_webpa_send_message_request,
        (caddr_t) &req, (xdrproc_t) xdr_webpa_send_message_response, (caddr_t) &res, timeout);
    
    // clear SIGPIPE before unblocking
    if (st != RPC_SUCCESS)
    {
      WebPA_Server_ClearPendingSignal(SIGPIPE);
    }
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
  WalInfo("destroying client: %p\n", c);
  
  if (c == NULL)
  {
    WalError("Client destroy client is NULL\n");
    return;
  } 

  if (c->topic)   free(c->topic);
  if (c->proto)   free(c->proto);
  if (c->host)    free(c->host);
  if (c->clnt)    clnt_destroy(c->clnt);
  
  pthread_cond_destroy(&c->cond);
  pthread_mutex_destroy(&c->mutex);
  
  free(c);  
}

static WebPA_Client* WebPA_Client_Create(webpa_register_request* req)
{
  WebPA_Client* c = (WebPA_Client *) malloc(sizeof(WebPA_Client));
  
  if (c)
  {
    if (req->topics.topics_len > 0)
      c->topic = strdup(req->topics.topics_val[0].topic);
    else
      c->topic = NULL;

    c->prog_num = req->prog_num;
    c->prog_vers = req->prog_vers;

    if (req->proto)
      c->proto = strdup(req->proto);
    else
      c->proto = NULL;

    if (req->host)
      c->host = strdup(req->host);
    else
      c->host = NULL;

    c->clnt = NULL;
    c->keep_alive_interval = 1;

    pthread_mutex_init(&c->mutex, NULL);
    pthread_cond_init(&c->cond, NULL);

    c->queue = NULL;
    c->is_dead = 0;
    c->worker_thread = (pthread_t) 0;
  }
  else
  {
    WalError("Malloc failed client not allocated\n");
  }
  
  return c;
}

static void* WebPA_ServiceClient(void* argp)
{
  int                 i;
  int                 n;
  WebPA_Client*       c;
  enum clnt_stat      st;
  struct timespec     ts;
  struct timespec     now;
  message_queue*      p;
  message_queue*      q;

  i = 0;
  n = 0;
  c = NULL;
  st = 0;
  memset(&ts, 0, sizeof(struct timespec));
  memset(&now, 0, sizeof(struct timespec));
  p = NULL;
  q = NULL;

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
  c->clnt = clnt_create(c->host, c->prog_num, c->prog_vers, c->proto);
  if (!c->clnt)
  {
    char const* err = clnt_spcreateerror(c->host);
    WalError("failed to create client to (prog:%d vers:%d proto:%s): %s\n",
      c->prog_num, c->prog_vers, c->proto, err);
  }

  while (c->clnt != NULL)
  {
    st = RPC_FAILED;
    pthread_mutex_lock(&c->mutex);
    
    clock_gettime(CLOCK_REALTIME, &now);
    ts.tv_sec = now.tv_sec + 1;

    n = pthread_cond_timedwait(&c->cond, &c->mutex, &ts);
    if (c->is_dead)
    {
      WalInfo("got shutdown signal for %p\n", c);
      pthread_mutex_unlock(&c->mutex);
      goto client_shutdown;
    }

    if (n == ETIMEDOUT)
    {
      struct timeval timeout;
      timeout.tv_sec = 2;
      timeout.tv_usec = 0;

      st = clnt_call(c->clnt, NULLPROC, (xdrproc_t) xdr_void, (caddr_t) NULL,
          (xdrproc_t) xdr_void, (caddr_t) NULL, timeout);
    }
    else
    {
      if (c->queue)
      {
        p = NULL;
        q = c->queue;
        while (q)
        {
          st = WebPA_Client_SendMessage(c, q->buff, q->n);
          //update message status
          pthread_mutex_lock(&msgStatusMutex);
          if(st != RPC_SUCCESS)
          {
            msgStatus = WAL_FAILURE;
          }
          else
          {
            msgStatus = WAL_SUCCESS;
          }
          pthread_cond_signal(&msgStatusCond);
          pthread_mutex_unlock(&msgStatusMutex);
          p = q;
          q = q->next;

          free(p->buff);
          free(p);
        }
        c->queue = NULL;
      }
    }

    if (st != RPC_SUCCESS)
    {
      WebPA_Server_ClearPendingSignal(SIGPIPE);

      char const* s = clnt_sperrno(st);
      WalError("clnt_call failed: %s\n", s);
      pthread_mutex_unlock(&c->mutex);
      break;
    }
    pthread_mutex_unlock(&c->mutex);
  }

client_shutdown:
  WalInfo("cleaning up after client: %p\n", c);
  pthread_mutex_lock(&mutex);
  for (i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients[i] == c)
      clients[i] = NULL;
  }

  WebPA_Client_Destroy(c);
  pthread_mutex_unlock(&mutex);
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

  // TODO: do we really care what goes back to client?
  if (res)
    res->ack = 0;

  return TRUE;
}

// client is registering it's embedded rpc server with webpa
bool_t
webpa_register_1_svc(webpa_register_request req, webpa_register_response* res, struct svc_req* svc)
{
  int             i;
  int             n;
  int             index;
  pthread_t       thr;
  pthread_t*      dead_clients;
  WebPA_Client*   client;
 
  (void) svc;

  client = NULL;
  i = 0;
  n = 0;
  index = 0;
  thr = (pthread_t) 0;
  dead_clients = (pthread_t *) malloc(sizeof(pthread_t) * MAX_CLIENTS);

  /* always hold the clients[] list mutex when enumerating */
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
        WalInfo("client (prog:%d vers:%d proto:%s already registered, clearing",
          req.prog_num, req.prog_vers, req.proto);

        /* signal to client service thread that the client is being re-used */
        pthread_mutex_lock(&clients[i]->mutex);
        clients[i]->is_dead = 1;
        dead_clients[n++] = clients[i]->worker_thread;
        pthread_cond_signal(&clients[i]->cond);
        pthread_mutex_unlock(&clients[i]->mutex);

        WalInfo("client cleared, continuing with new registration\n");
      }
    }
  }

  /* release clients[] list mutex to allow for any pending client service
   * threads to exit. those thread will need to acquire this mutex in
   * order to remove themselves from that list
   */
  pthread_mutex_unlock(&mutex);

  /* wait for any threads that are still servicing dead clients to exit */
  for (i = 0; i < n; ++i)
  {
    int ret;
    WalInfo("waiting for thread: %ld to exit\n", dead_clients[i]);
    ret = pthread_join(dead_clients[i], NULL);
    WalInfo("joined thread: %ld\n", ret);
  }

  free(dead_clients);

  /* mutex must be held while enumerating the clients[] list */
  pthread_mutex_lock(&mutex);
  for (i = 0, index = -1; index == -1 && i < MAX_CLIENTS; ++i)
  {
    if (!clients[i])
      index = i;
  }

  if (index != -1)
  {
    client = WebPA_Client_Create(&req);
    if (client)
    {
      clients[index] = client;
      WalInfo("client (prog:%d vers:%d proto:%s already registered in slot: %d with topic: %s\n",
          req.prog_num, req.prog_vers, req.proto, index, client->topic);
    }
  }

  if (index == -1)
  {
    WalError("failed to find index for new client");
    pthread_mutex_unlock(&mutex);
    return FALSE;   
  }

  if (!client)
  {
    WalError("failed to create client. NULL\n");
    pthread_mutex_unlock(&mutex);
    return FALSE;
  }

  WalInfo("creating thread for new client\n");
  pthread_create(&thr, NULL, WebPA_ServiceClient, client);
  client->worker_thread = thr;
  pthread_mutex_unlock(&mutex);
 
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
