/*For implementation of the queue 'Data structures, Algorithms, and Applications' by Sartaj Sahni has been referred*/
#ifndef QUEUE_
#define QUEUE_

template <class T> class queue;

template <class T>
class element
{
    friend class queue<T>;
    private:
    T data;
    int size;
    element<T> *link;
};

template <class T>
class queue
{
    public :
    queue(){front=rear=0;}		//Constructor
    ~queue();				//Destructor
    bool empty() const
    {
    bool result;
 pthread_mutex_lock(&q_mutex);
	result=(front)? false:true;
pthread_mutex_unlock(&q_mutex);
	return result;
    }
    int shrt() const;
    queue<T>& add(const T& x,int y);	//Adds the element to the rear end of the queue.
    queue<T>& get(T& x,int& y);		//This reads from front of the queue. Can be used for FCFS
    queue<T>& getshrt(T& x,int& y);	// This reads the shortest element from the queue.This can be used for SJF
    private:
    element<T> *front;
    element<T> *rear;
   mutable pthread_mutex_t q_mutex;
};

template<class T>
queue<T>::~queue()
{
    element<T> *next;
    while(front)
    {
        next=front->link;
        delete front;
        front=next;

    }
 pthread_mutex_destroy(&q_mutex);
}

template<class T>
        queue<T>& queue<T>::add(const T& x, int y)
{

    element<T> *p= new element<T>;
    p->data = x;
    p->link = 0;
    p->size=y;
    pthread_mutex_lock(&q_mutex);
    if (front)
	{
		rear->link=p;
	}
    else front = p;
    rear = p;
   pthread_mutex_unlock(&q_mutex);
    return *this;
}

template<class T>
        queue<T>& queue<T>:: get(T& x, int& y)
{
	pthread_mutex_lock(&q_mutex);
    x=front->data;
    y=front->size;
    element<T> *p = front;
    front = front->link;
    delete p;
  pthread_mutex_unlock(&q_mutex);
    return *this;
}
template <class T>
int queue<T>::shrt() const
{
	if(empty())
	{
		throw 20;
	}
	element<T> *p=front;
	int small=p->size;
	int index=1;
	while(p)
	{
		if (small >= p->size)
		{
			small=p->size;
		}
	p=p->link;
	}
	element<T> *q=front;
	while(q)				//This will return the index of the first of the smallest elements.
	{					//suppose there are 2 elements with smallest size, it will return
		if (q->size == small)		//only the first of the smallest elements.
		{
			break;
		}
	index=index+1;
	q=q->link;
	}
//	std::cout<<"inside shrt()"<<std::endl;
	return index;
}

template <class T>
queue<T>& queue<T>::getshrt(T& x, int& y)
{
	pthread_mutex_lock(&q_mutex);
	element<T> *p=front;
	int k=shrt();
//	std::cout<<"In getshrt()"<<std::endl;
	element<T> *q = front;
        if (k == 1)
	{
	front=front->link; 		//if first element is shortest then remove it from the linkedlist. 						// p is already direced to first element.
	}
        else
	{
		int i=1;
		while (i<(k-1))
		{
		p=p->link;  		//this stops at one element before the shortest element
		i++;
		}
		q=p->link;		//now q points to shortest element
		p->link = q->link;	//removing shortest from the chain
	}
		x=q->data;
		y=q->size;
		delete q;
//	std::cout<<"In getshrt): after delete"<<std::endl;
		pthread_mutex_unlock(&q_mutex);
		return *this;
}



#endif /* QUEUE_ */
