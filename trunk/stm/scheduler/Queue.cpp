#include "Queue.h"

using namespace stm::scheduler;

int InnerJob::m_iAllJobsIDs = 0;

Queue::~Queue()
{ 
    while (first!=0)
	{
        Node* temp = first;
        first = first->next;
        delete temp;
    }
    mySize=0;
}

Queue::Queue(const Queue &original) : first(0), last(0), mySize(0)
{
    Node* source = original.first;
    while (source != 0)
	{
        Node* temp;
        temp = new Node;
        temp->data = source->data;
        temp->next = 0;
        if (last != 0)
		{
            last->next = temp;
        }
		else
		{
            first = temp;
            last = temp;
        }
        last = temp;
        source = source->next;
        mySize++;
    }
}

void Queue::push(InnerJob *const value)
{
	Node* temp = new Node;
	if (temp != 0)
	{
		temp->data = value;
		temp->next = 0;
        if (!empty())
		{
			last->next = temp;
		}
		else
		{
			first = temp;
		}
		last = temp;
		mySize++;
    }
	else
	{
		//cerr << "*** Out of memory -- cannot push ***\n";
	}
}

void Queue::pop()
{
	if (!empty())
	{
		Node* temp = first;
		first = first->next;
		if (first == 0)
			last = 0;
		delete temp;
		mySize--;
	}
	else
	{
		//cerr << "*** Stack is empty -- cannot pop ***\n";
	}
}

