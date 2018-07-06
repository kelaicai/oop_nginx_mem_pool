//nginx�ڴ����ҵ
#define _CRT_SECURE_NO_WARNINGS

#include<iostream>
#include<stdlib.h>
#include<string.h>
#include<assert.h>

using namespace std;


//�ڴ���� 
//#define ngx_memalign(alignment, size, log)  ngx_alloc(size, log)
//����ܷ���� 
#define NGX_MAX_ALLOC_FROM_POOL  4095 

#define NGX_ALIGINMENT sizeof(unsigned long)  //8�ֽڶ���




typedef unsigned char u_char;
//���е��ǵ�ַ����



typedef unsigned int ngx_uint_t;

typedef struct ngx_pool_s ngx_pool_t;

typedef struct ngx_pool_cleanup_s  ngx_pool_cleanup_t;

typedef struct ngx_pool_large_s ngx_pool_large_t;
//typedef unsigned int ngx_uint_t;

/*
С���ڴ�chunk��ͷ��Ϣ
*/
typedef struct {
	u_char               *last;
	u_char               *end;
	ngx_pool_t           *next;
	ngx_uint_t            failed;   //С���ڴ����ʧ�ܴ��� 
} ngx_pool_data_t;
/*
��ʾnginx�ڴ�صĴ�����
*/
struct ngx_pool_s {
	ngx_pool_data_t       d;  //�ڴ�����ݿ� 
	size_t                max;
	ngx_pool_t           *current;
	ngx_pool_large_t     *large;
	ngx_pool_cleanup_t *cleanup;  //�ͷ��ڴ� 
};
typedef struct ngx_pool_s ngx_pool_t;

/*
nginx����ڴ�������Ϣ
*/
struct ngx_pool_large_s {
	ngx_pool_large_t     *next;
	void                 *alloc;
};

//typedef ngx_poll_large_s ngx_pool_large_t;

//�����ڴ�
typedef void(*ngx_pool_cleanup_pt)(void *data);
struct ngx_pool_cleanup_s {
	ngx_pool_cleanup_pt   handler;  //�ص�����
	void                 *data;      //����Ҫ���������
	ngx_pool_cleanup_t   *next;      //ָ����һ������ṹ��
};




class NgxMemPool
{
public:
	NgxMemPool(size_t size);
	~NgxMemPool();
	//����ngx���ڴ��
	void ngx_create_pool(size_t size);
	//����ngx�ڴ��
	void ngx_destroy_pool();
	//�����ڴ��
	void ngx_reset_pool();
	//�����ڴ棬����
	void* ngx_palloc(size_t size);
	//�����ڴ棬������
	void* ngx_pnalloc(size_t size);
	//���ڴ�黹���ڴ��
	void ngx_pfree(void *p);
	//�����ڴ��ڴ�� 
	void set_pool(ngx_pool_t *pool);
	//�ڴ���� 
	//С�ڴ���� 
	void* ngx_palloc_small(size_t size, ngx_uint_t aligin);
	//���ڴ����
	void* ngx_palloc_large(size_t size);
	//�ڴ���С������ʹ�øú����ٷ���

	void *ngx_memaligin(size_t size);  //alloc�ײ���ʵ���Ƕ�GIibc��malloc�����˷�װ������size���ɶ�̬�����ڴ�
	void* ngx_alloc(size_t size); //void* NgxMemPool::alloc(size_t size)
	void* ngx_palloc_block(size_t size);
	void ngx_free(void *p)
	{
		free(p);
	}
private:
	ngx_pool_t *_pool;
	u_char * ngx_aligin_ptr(u_char* p, unsigned int a)
	{
		return (u_char *)(((uintptr_t)(p)+((uintptr_t)a - 1)) & ~((uintptr_t)a - 1));
	}



};

NgxMemPool::NgxMemPool(size_t size = 10) :_pool(NULL)
{
	ngx_create_pool(size);
}


NgxMemPool::~NgxMemPool()
{
	cout << "��ʼ����" << endl;
	ngx_destroy_pool();
	cout << " ~s" << endl;
}
//���� 
void* NgxMemPool::ngx_memaligin(size_t size)
{
	return ngx_alloc(size);
}

void* NgxMemPool::ngx_alloc(size_t size)
{
	void *p = malloc(size);
	if (p == NULL)
	{
		cout << "malloc error" << endl;
	}
	return p;
}

//�ڴ治����ʱ�򣬶�������
void* NgxMemPool::ngx_palloc_block(size_t size)
{
	u_char *m;
	size_t psize;
	ngx_pool_t *p, *New;

	p = _pool;
	psize = (size_t)(p->d.end - (u_char*)_pool);
	m = (u_char*)(this->ngx_memaligin(psize)); //����͵�һ���ڴ��С��ͬ���ڴ��
	if (m == NULL)
	{
		return NULL;
	}
	cout << "vloc" << endl;
	New = (ngx_pool_t *)m;
	//�����µ�end 
	New->d.end = m + psize;
	New->d.next = NULL;
	New->d.failed = 0;

	m += sizeof(ngx_pool_data_t);
	m = ngx_aligin_ptr(m, NGX_ALIGINMENT);
	New->d.last = m + size;

	//��һ����ǰ�ڴ���е�����ڵ㣬�鿴�ڴ��Ƿ񹻷��䣬���������������ε�ʱ����ȥ��һ���ڴ��Ѱַ
	for (p = _pool->current; p->d.next;p= p->d.next)
	{
		if (p->d.failed++>4)
		{
			_pool->current = p->d.next;
		}
	}

	
	p->d.next = New;  //���ýڵ���뵽������
	return m;
}
void NgxMemPool::set_pool(ngx_pool_t *pool)
{
	_pool = pool;
}

//����ngx���ڴ��
void NgxMemPool::ngx_create_pool(size_t size)
{
	ngx_pool_t *p;

	p = (ngx_pool_t*)ngx_memaligin(sizeof(ngx_pool_t)+size);
	if (p == NULL)
	{
		_pool = NULL;
		return;
	}

	p->d.last = (u_char*)p + sizeof(ngx_pool_t); //last�տ�ʼ����ngx_pool_t��β��
	p->d.end = (u_char*)p + size;  //ָ�����size��С����ڴ�β�� 
	p->d.next = NULL;
	p->d.failed = 0;

	size = size / 2;  //����1/2��Ϊ����ڴ�;
	p->max = (size<NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;  //�Ƚ��ǲ��Ǵ�������4095

	p->large = NULL;
	p->cleanup = NULL;
	p->current = p;
	_pool = p; //����ǰ������ڴ��ָ���������� 

	//�����ɹ�
	cout << "success" << endl;
}
//����ngx�ڴ��
void  NgxMemPool::ngx_destroy_pool()
{
	ngx_pool_t *p, *n;
	ngx_pool_large_t *l;
	ngx_pool_cleanup_t *c;

	//�ͷ�С���ڴ� 
	/*
	for(c=_pool->cleanup;c;c=c->next)
	{
	if(c->handler)
	{
	c->handler(c->data);
	}
	}
	*/
	//�ͷŴ���ڴ� 
	//	cout<<"s"<<endl; 
	cout << "large begin" << endl;
	for (l = _pool->large; l; l = l->next)
	{
		if (l->alloc)
		{
			
			ngx_free(l->alloc);
			l->alloc = NULL;
		}
	}
	cout << "large end" << endl;
	//�ͷ�С���ڴ� 

	printf("%p\n", _pool);
	printf("%p\n", _pool->d.next);
	cout << "s" << endl;


	printf("%p\n", _pool);
	if (!_pool->d.next)  //��ǰֻ��һ�� 
	{
		_pool = NULL;
		return;
	}

	//	cout<<"s"<<endl; 
	/*
	for (n = _pool->d.next, p = n; ; n = p, n = n->d.next)
	{

		if (p == NULL)
		{
			break;
		}
		ngx_free(n);
	}*/


	for (p=_pool,n = _pool->d.next;; p=n, n = n->d.next)
	{
		ngx_free(p);
		if (n == NULL)
		{
			break;
		}
		
	}
	//n = NULL;

	//	cout<<"s"<<endl; 
	//ngx_free(_pool);
	_pool = NULL;
	cout << "�ͷŽ���" << endl;


}
//�����ڴ��
void  NgxMemPool::ngx_reset_pool()
{
	ngx_pool_t *p;
	ngx_pool_large_t *l;

	for (l = _pool->large; l; l = l->next)
	{
		if (l->alloc)
		{
			ngx_free(l);
		}
	}

	for (p = _pool; p; p = p->d.next)
	{
		p->d.last = (u_char*)p + sizeof(ngx_pool_t);
		p->d.failed = 0;
	}

	_pool->current = _pool;
	_pool->large = NULL;
}
//�����ڴ棬����
void*  NgxMemPool::ngx_palloc(size_t size)
{
	if (size<_pool->max)
	{
		return ngx_palloc_small(size, 1);
	}
	else
	{
		return ngx_palloc_large(size);
	}
}
//�����ڴ棬������
void*  NgxMemPool::ngx_pnalloc(size_t size)
{
	if (size<this->_pool->max)
	{
		return ngx_palloc_small(size, 0);
	}
	else
	{
		return ngx_palloc_large(size);
	}
}
//���ڴ�黹���ڴ��
void  NgxMemPool::ngx_pfree(void *p)
{
	assert(p != NULL);
	ngx_pool_large_t *l;

	if (p == NULL)
	{
		return;
	}
	for (l = _pool->large; l; l = l->next)
	{
		if (p == l->alloc)
		{
			ngx_free(l->alloc);
		}
		l->alloc = NULL;
	}
}

//С�ڴ���� 
void* NgxMemPool::ngx_palloc_small(size_t size, ngx_uint_t aligin)
{
	u_char* m;
	ngx_pool_t *p;

	p = _pool->current;
	do{
		m = p->d.last;

		if (aligin)
		{
			m = ngx_aligin_ptr(m, NGX_ALIGINMENT); //��ַ���� 
		}

		if ((size_t)(p->d.end - m) >= size)
		{
			p->d.last = m + size;
			cout << "small" << endl;
			return m;
		}

		p = p->d.next;
	} while (p);

	//Ҫ������ڴ�ܴ�

	cout << "upper than small" << endl;
	return	ngx_palloc_block(size);
}
//���ڴ����
void* NgxMemPool::ngx_palloc_large(size_t size)
{
	void *p;
	ngx_uint_t n;
	ngx_pool_large_s *large;

	p = ngx_alloc(size);

	if (p == NULL)
	{
		return NULL;
		//	_pool->large->alloc= NULL;
	}

	n = 0;

	//����ҵ����е�large�飬�������ֱ�ӰѸո�������ڴ棬����������
	for (large = _pool->large; large; large = large->next)
	{
		if (large->alloc == NULL)
		{
			large->alloc = p;
			return p;
		}

		//Ϊ���ṩЧ�ʣ����3�λ�û�п��е�large�飬�򴴽�һ��
		if (n++>3)
		{
			break;
		}
	}

	//����large��Ĵ���ʹ�õ�ʱС���ڴ�������
	large = (ngx_pool_large_s*)ngx_palloc_small(sizeof(ngx_pool_large_t), 1);
	if (large == NULL)
	{
		ngx_free(p);
		return NULL;
	}


	//�������������� 
	large->alloc = p;
	large->next = _pool->large;
	_pool->large = large;

	cout << "large block" << endl;
	return p;

}

#if 1

int main()
{
	NgxMemPool ngx(428);
	cout << "su" << endl;
	//ngx.ngx_palloc(400);
	
	for (int i = 0; i < 100; ++i)
	{
		int n = rand() % 1000 + 4;
		char* p = (char*)ngx.ngx_palloc(n);
		cout << i << " " << n << endl;
		strcpy(p, "hello");
	}

	system("pause");
	return 0;

}

#endif