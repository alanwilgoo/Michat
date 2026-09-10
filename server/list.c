#include"list.h"
/*
用单链表封装所有连接成功的客户端信息
*/
list *list_init()
{
    list *head=malloc(sizeof(list));
    head->next=NULL;
    return head;
}
int list_insert_tail(list *newnode,list *head)
{
    list *p=head;
    while(p->next!=NULL)
    {
        p=p->next;
    }
    p->next=newnode;
    return 0;
}
int list_delete(char *delip,unsigned short delport,list *head)
{
    list *p=head->next;
    list *q=head;
    while(p!=NULL)
    {
        if(strcmp(p->ip,delip)==0&&p->port==delport)
        {
            q->next=p->next;
            free(p);
            return 0;

        }
        q=p;
        p=p->next;
    }
    return -1;
}

int list_delete_node(list *node, list *head)
{
    list *current = head->next;
    list *previous = head;
    while(current != NULL) {
        if(current == node) {
            previous->next = current->next;
            free(current);
            return 0;
        }
        previous = current;
        current = current->next;
    }
    return -1;
}
