#include "main.h"
#include "list.h"
#include "bitmap.h"
#include "hash.h"
//function option: 1 = create, 2 = dump 3 = delte 4 = else

void list_func(char arg[][30] , int op);
void bitmap_func(char arg[][30], int op);
void hash_func(char arg[][30], int op);
void create(char* );
void dumpdata(char* );
void delete(char *);


int main(){
    
    bitmap_head=NULL;
	hash_head=NULL;
	listtable_head=NULL;

    char command[300];
    fgets(command,300,stdin);
    command[strlen(command)-1] = '\0';

    /*enter the command while command = "quit"*/
    while (strcmp("quit",command)){
        if(!strncmp(command, "create",6)) create(command);
        else if(!strncmp(command, "dumpdata",8)) dumpdata(command);
        else if(!strncmp(command, "delete",6)) delete(command);
        else if(!strncmp(command,"list",4)) list_command(command);
        else if(!strncmp(command,"bitmap",6)) bitmap_command(command);
        else if(!strncmp(command,"hash",4)) hash_command(command);

        fgets(command, 300, stdin);
        command[strlen(command)-1] = '\0';
    }

    return 0;
    
}
//create command
void create(char* command){
    char arg[8][30] = {'\0',};
    char* token = strtok(command, " ");
    int i = 1;
    /*save commands separated by spaces(' ')*/
	while(token != NULL){
		strcpy(arg[i], token);
		token = strtok(NULL, " ");
		i++;
	}
    
    if(!strncmp(arg[2],"list",4)) list_func(arg,1); //create list
    else if(!strncmp(arg[2],"bitmap",6)) bitmap_func(arg,1); //create bitmap
    else if(!strncmp(arg[2],"hashtable", 9)) hash_func(arg,1);// create hash

}

/*dump the data*/
void dumpdata(char* command){

    char arg[8][30] = {'\0',};
    char* token = strtok(command, " ");
    int i = 1;
    /*save commands separated by spaces(' ')*/
    	while(token != NULL){
		strcpy(arg[i], token);
		token = strtok(NULL, " ");
		i++;
	}     

    struct bitmap_table* bt = bitmap_head;
	struct hash_table* ht = hash_head;
	struct list_table* lt = listtable_head;

    /*find the correct data type and link to the correct function*/

	while(bt != NULL){
		if(!strcmp(bt->name, arg[2])){
			bitmap_func(arg,2);
            return;
		}
		else
			bt = bt->link;	
	}

    while(ht != NULL){
		if(!strcmp(ht->name, arg[2])){
			hash_func(arg,2);
            return;
		}
		else
			ht = ht->link;	
	}

    while(lt != NULL){
		if(!strcmp(lt->name, arg[2])){
			list_func(arg,2);
            return;
		}
		else
			lt = lt->link;	
	}
}
/*delete the data structure*/
void delete(char* command){
    char arg[8][30] = {'\0',};
    char* token = strtok(command, " ");
    int i = 1;
    /*save commands separated by spaces(' ')*/
	while(token != NULL){
		strcpy(arg[i], token);
		token = strtok(NULL, " ");
		i++;
	}   

    struct bitmap_table* bt = bitmap_head;
	struct hash_table* ht = hash_head;
	struct list_table* lt = listtable_head;

	while(bt != NULL){
		if(!strcmp(bt->name, arg[2])){
			bitmap_func(arg,3);
            return;
		}
		else
			bt = bt->link;	
	}

    /*find the correct data type and link to the correct function*/
    while(ht != NULL){
		if(!strcmp(ht->name, arg[2])){
			//printf("dump bitmap\n");
			hash_func(arg,3);
            return;
		}
		else
			bt = bt->link;	
	}

    while(lt != NULL){
		if(!strcmp(lt->name, arg[2])){
			//printf("dump bitmap\n");
			list_func(arg,3);
            return;
		}
		else
			lt = lt->link;	
	}


}

/*match to correct action by using parameter 'option'*/
void list_func(char arg[][30], int option){
    //function option: 1 = create, 2 = dump 3 = delte
    struct list_table* lt;
    struct list_table* ptr;
    struct list_table* ptr2;
    struct list_elem * target;
    struct list_item* item;
    switch (option)
    {
        case 1: //option = create
            
            lt = (struct list_table*)malloc(sizeof(struct list_table));
            lt->list = (struct list*)malloc(sizeof(struct list));
            strcpy(lt->name, arg[3]);
            lt->link = NULL;

            list_init(lt->list);
            
            /*if listtable_head alreay have list?*/
            if(!listtable_head) listtable_head = lt; 
            else{
                for(ptr = listtable_head; ptr->link != NULL; ptr = ptr->link) ;
                ptr->link = lt;
            }
            break;

        case 2:  //option = dump
            /*Find the correct list and dump the data in the list*/
            for(ptr = listtable_head; ptr != NULL; ptr = ptr->link){
                if(!strcmp(arg[2], ptr->name)){
                    list_dump(ptr->list);
                    break;
                }
            }
            break;

        case 3: //option = delete

            /*Find the correct list and delete the data in the list*/
            for(ptr = listtable_head; ptr != NULL; ptr = ptr->link){
                if(!strcmp(arg[3], ptr->name)){
                    target = list_begin(ptr->list);
                    while(!(list_empty(ptr->list))){
                        target = list_pop_front(ptr->list);
                        item = list_entry(target, struct list_item, elem);
                        target = list_begin(ptr->list);
                        free(item);
                    }
                    
                    if(ptr == listtable_head){
                        free(ptr); 
                        listtable_head = NULL;
                    }
                    else{
                        
                        for(ptr2 = listtable_head; ptr2->link != NULL; ptr2 = ptr2->link) ;
                        ptr2->link = ptr2->link->link;
                        free(ptr2);
                    }
                    break;
                }
            }
            break;

        default: 
            break;
    }

}

/*match to correct action by using parameter 'option'*/
void bitmap_func(char arg[][30], int option){
    //function option: 1 = create, 2 = dump 3 = delete

    struct bitmap_table* bt;
    struct bitmap_table* ptr;
    struct bitmap_table* ptr2;
    switch (option)
    {
        case 1: 
            //bitmap create command
            bt = (struct bitmap_table*)malloc(sizeof(struct bitmap_table));

            strcpy(bt->name, arg[3]);
            bt->bitmap = bitmap_create(atoi(arg[4]));
            bt->link = NULL;

            /*if bitmap_head alreay have bitmap?*/
            if(bitmap_head == NULL) bitmap_head = bt;
            else{
                for(ptr = bitmap_head; ptr->link != NULL; ptr = ptr->link) ;
                ptr->link = bt;
            }
            break;

        case 2:  //dump
            /*Find the correct bitmap and dump all bits in the bitmap*/
            for(ptr = bitmap_head; ptr != NULL; ptr = ptr->link){
                if(!strcmp(arg[2], ptr->name)){
                    bitmap_dumpdata(ptr->bitmap);
                    break;
                }
            }
            break;

        case 3: //delete
            /*Find the correct bitmap and delete the data in the bitmap*/
            for(ptr = bitmap_head; ptr != NULL; ptr = ptr->link){
                if(!strcmp(arg[2],ptr->name)){
                    bitmap_destroy(ptr->bitmap);
                    if(ptr == bitmap_head){
                        free(ptr); 
                        bitmap_head = NULL;
                    }
                    else{
                        

                        for(ptr2 = bitmap_head; ptr2->link != NULL; ptr2 = ptr2->link) ;
                        ptr2->link = ptr2->link->link;
                        free(ptr2);
                    }
                    break;
                }
            }
            break;

        default: break;
	}
        

}

/*match to correct action by using parameter 'option'*/
void hash_func(char arg[][30], int option){
    //function option: 1 = create, 2 = dump 3 = delte

    struct hash_table* ht;
    struct hash_table* ptr;
    struct hash_table* ptr2;
    switch (option)
    {
        case 1: //create hash

            ht = (struct hash_table*)malloc(sizeof(struct hash_table));
            ht->hash = (struct hash*)malloc(sizeof(struct hash));
            ht->link = NULL;
            strcpy(ht->name, arg[3]);
            hash_init(ht->hash, hash_function, hash_less, NULL);
            /*if hash_head alreay have hash?*/
            if(hash_head == NULL) hash_head = ht;
            else{
                for(ptr = hash_head; ptr->link != NULL; ptr = ptr->link) ;
                ptr->link = ht;
            }

            break;

        case 2:  //dump the hash

            /*Find the correct hash and dump all bits in the hash*/
            for(ptr = hash_head; ptr != NULL; ptr = ptr->link){
                if(!strcmp(arg[2],ptr->name)){
                    hash_dump(ptr->hash);
                    break;
                }
            }
            break;

        case 3: //delete the hash
            /*Find the correct hash and delete the data in the hash*/
            for(ptr = hash_head; ptr != NULL; ptr = ptr->link){
                if(!strcmp(arg[2],ptr->name)){
                    hash_destroy(ptr->hash, hash_free);
                    if(ptr == hash_head){
                        free(ptr); 
                        hash_head = NULL;
                    }
                    else{
                        
                        for(ptr2 = hash_head; ptr2->link != NULL; ptr2 = ptr2->link) ;
                        ptr2->link = ptr2->link->link;
                        free(ptr2);
                    }
                    break;
                }
            }
            break;

        default: break;
	}
        

}