/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define SBUF_SIZE 1000
#define NTHREADS 20
typedef struct item{
    int ID;
    int left_stock;
    int price;
    int readcnt;
} ITEM;

typedef struct tree_node{
    ITEM item;
    struct tree_node *left;
    struct tree_node *right;
} tree_node;
typedef tree_node *TREE_NODE;

typedef struct {
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
sbuf_t sbuf;
void echo(int connfd);
void* thread(void* vargp);
static void init_mutex();
void check_clients(int connfd);
static sem_t mutex;
int show_count = 0;
/*manage stock system*/
void show(TREE_NODE root, int connfd, char* stock_buf);
void buy(TREE_NODE root, int connfd, int id, int count);
void sell(TREE_NODE root, int connfd, int id, int count);
void read_stock_file();
void update_stock_file(FILE* fp, TREE_NODE root);
TREE_NODE root = NULL;

/*signal handler*/
void sigint_handler(int sig){
    FILE *fp;
    fp = fopen("stock.txt", "w");
    update_stock_file(fp, root);
    fclose(fp);
    exit(0);
}

int main(int argc, char **argv) 
{   
    Signal(SIGINT, sigint_handler);
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUF_SIZE);
    
    for(i = 0; i < NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }

    read_stock_file();
    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        sbuf_insert(&sbuf, connfd);

        FILE *fp;
        fp = fopen("stock.txt", "w");
        update_stock_file(fp, root);
        fclose(fp);
    }

    

    return 0;
}
/* $end echoserverimain */

void* thread(void* vargp){
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);
        check_clients(connfd);
        Close(connfd);
    }
    return NULL;

}

static void init_mutex(){
    Sem_init(&mutex, 0, 1);
}

void check_clients(int connfd)
{
    int n;
    char buf[MAXLINE];
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    Pthread_once(&once, init_mutex);

    Rio_readinitb(&rio, connfd);

    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        P(&mutex); 
        printf("Server received %d bytes\n", n);
        if(!strncmp(buf, "show", 4)){
            char stock_buf[MAXLINE];
            memset(stock_buf, 0, MAXLINE);
            show(root, connfd, stock_buf);
            Rio_writen(connfd, stock_buf, MAXLINE);
            V(&mutex);
        }
        else if(strncmp(buf, "buy", 3) == 0){ 
            char temp[5];
            int id, count;
            sscanf(buf, "%s %d %d", temp, &id, &count);
            buy(root, connfd, id, count);
        }
        else if(strncmp(buf, "sell", 4) == 0){
            int id, count;
            char temp[5];
            sscanf(buf, "%s %d %d", temp, &id, &count);
            sell(root, connfd, id, count);
        }
        else if(strncmp(buf, "exit", 4) == 0){
            V(&mutex);
            return;
        }
        else{
            printf("%s",buf);
            V(&mutex);
            Rio_writen(connfd, "Invalid command\n", 16);
        }
        
        
    }
}

void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;                    
    sp->front = sp->rear = 0;     
    Sem_init(&sp->mutex, 0, 1);   
    Sem_init(&sp->slots, 0, n);   
    Sem_init(&sp->items, 0, 0);   
}

void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);                    
    P(&sp->mutex);                    
    sp->buf[(++sp->rear)%(sp->n)] = item; 
    V(&sp->mutex);                    
    V(&sp->items);                    
}

int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);                    
    P(&sp->mutex);                    
    item = sp->buf[(++sp->front)%(sp->n)]; 
    V(&sp->mutex);                    
    V(&sp->slots);                    
    return item;
}

/*manage stock*/
void show(TREE_NODE root, int connfd, char *stock_buf){
    if(root == NULL) return;
    char buf[MAXLINE];
    show(root->left, connfd, stock_buf);
    sprintf(buf, "%d %d %d\n", root->item.ID, root->item.left_stock, root->item.price);
    strcat(stock_buf, buf);
    show(root->right, connfd, stock_buf);

    return;
}

void buy(TREE_NODE root, int connfd, int id, int count){
    if(root == NULL) return;
    if(root->item.ID == id){
        if(root->item.left_stock < count){
            V(&mutex);
            Rio_writen(connfd, "Not enough left stock\n", MAXLINE);
        }
        else{
            root->item.left_stock -= count;
            V(&mutex);
            Rio_writen(connfd, "[buy] success\n", MAXLINE);
        }
    }
    else if(id > root->item.ID) buy(root->right, connfd, id, count);
    else buy(root->left, connfd, id, count);
}

void sell(TREE_NODE root, int connfd, int id, int count){
    if(root == NULL) return;
    if(root->item.ID == id){
        root->item.left_stock += count;
        V(&mutex);
        Rio_writen(connfd, "[sell] success\n", MAXLINE);
    }
    else if(id > root->item.ID) sell(root->right, connfd, id, count);
    else sell(root->left, connfd, id, count);
}

void read_stock_file(){
    FILE *fp;
    char buf[MAXLINE];
    fp = fopen("stock.txt", "r");
    if(fp == NULL){
        printf("FILE OPEN ERROR: stock.txt\n");
        return;
    }

    while(fgets(buf, MAXLINE, fp) != NULL){
        TREE_NODE new_node = (TREE_NODE)Malloc(sizeof(tree_node));
        new_node->item.readcnt = 0;
        sscanf(buf, "%d %d %d", &new_node->item.ID, &new_node->item.left_stock, &new_node->item.price);
        new_node->left = NULL;
        new_node->right = NULL;

        if(root == NULL) root = new_node;
        else{
            TREE_NODE ptr = root;
            TREE_NODE prev = NULL;
            while(ptr){
                prev = ptr;
                if(ptr->item.ID < new_node->item.ID) ptr = ptr->right;
                else ptr = ptr->left;
            }
            if(new_node->item.ID < prev->item.ID) prev->left = new_node;
            else prev->right = new_node;
        }
    }
    
}

void update_stock_file(FILE* fp, TREE_NODE root){
    if(root == NULL) return;

    fprintf(fp, "%d %d %d\n", root->item.ID, root->item.left_stock, root->item.price);

    update_stock_file(fp, root->left);
    update_stock_file(fp, root->right);
}
