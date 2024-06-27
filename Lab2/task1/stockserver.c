/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
typedef struct item{
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
} ITEM;

typedef struct tree_node{
    ITEM item;
    struct tree_node *left;
    struct tree_node *right;
} tree_node;
typedef tree_node *TREE_NODE;

typedef struct {
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

void echo(int connfd);
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);

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
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }

    read_stock_file();

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &pool.ready_set)){ //new listenfd connection
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
        
            add_client(connfd, &pool); //add client to pool
        }
        check_clients(&pool);

        FILE* fp = fopen("stock.txt", "w");
        if(fp == NULL){
            printf("FILE OPEN ERROR: stock.txt\n");
            return 0;
        }
        update_stock_file(fp, root);
        fclose(fp);
    }
    exit(0);
}


void init_pool(int listenfd, pool *p)
{
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++) p->clientfd[i] = -1; 
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p)
{
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++){  
        if (p->clientfd[i] < 0) { 
            p->clientfd[i] = connfd;                    
            Rio_readinitb(&p->clientrio[i], connfd);     

            FD_SET(connfd, &p->read_set); 

            if (connfd > p->maxfd) p->maxfd = connfd;
            if (i > p->maxi) p->maxi = i;
            break;
        }
    }

    if (i == FD_SETSIZE) app_error("add_client error: Too many clients");
}

void check_clients(pool *p)
{
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) { //nready: number of ready descriptors -> nready = 0 when all clients are checked
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) { 
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE))) { 
                printf("Server received %d bytes\n", n);

                if(!strncmp(buf, "show", 4)){
                    char stock_buf[MAXLINE];
                    memset(stock_buf, 0, MAXLINE);
                    show(root, connfd, stock_buf);
                    Rio_writen(connfd, stock_buf, MAXLINE);
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
                    Close(connfd);
                    FD_CLR(connfd, &p->read_set);
                    p->clientfd[i] = -1;
                    return;
                }
                else{
                    printf("%s",buf);
                    Rio_writen(connfd, "Invalid command\n", 16);
                }

            }
            else { // EOF detected, close connection
                Close(connfd);
                FD_CLR(connfd, &p->read_set); 
                p->clientfd[i] = -1;
            }
        }
    }
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
            Rio_writen(connfd, "Not enough left stock\n", MAXLINE);
        }
        else{
            root->item.left_stock -= count;
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
        Sem_init(&new_node->item.mutex, 0, 1);
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
