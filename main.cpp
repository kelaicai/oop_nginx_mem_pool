//nginx内存池作业
#define _CRT_SECURE_NO_WARNINGS

#include<iostream>
#include<stdlib.h>
#include<string.h>
#include<assert.h>

using namespace std;


//内存对齐 
//#define ngx_memalign(alignment, size, log)  ngx_alloc(size, log)
//最大能分配的 
#define NGX_MAX_ALLOC_FROM_POOL  4095 

#define NGX_ALIGINMENT sizeof(unsigned long)  //8字节对齐




typedef unsigned char u_char;
//进行的是地址对齐



typedef unsigned int ngx_uint_t;

typedef struct ngx_pool_s ngx_pool_t;

typedef struct ngx_pool_cleanup_s  ngx_pool_cleanup_t;

typedef struct ngx_pool_large_s ngx_pool_large_t;
//typedef unsigned int ngx_uint_t;

/*
小块内存chunk的头信息
*/
typedef struct {
	u_char               *last;
	u_char               *end;
	ngx_pool_t           *next;
	ngx_uint_t            failed;   //小块内存分配失败次数 
} ngx_pool_data_t;
/*
表示nginx内存池的大类型
*/
struct ngx_pool_s {
	ngx_pool_data_t       d;  //内存池数据块 
	size_t                max;
	ngx_pool_t           *current;
	ngx_pool_large_t     *large;
	ngx_pool_cleanup_t *cleanup;  //释放内存 
};
typedef struct ngx_pool_s ngx_pool_t;

/*
nginx大块内存类型信息
*/
struct ngx_pool_large_s {
	ngx_pool_large_t     *next;
	void                 *alloc;
};

//typedef ngx_poll_large_s ngx_pool_large_t;

//清理内存
typedef void(*ngx_pool_cleanup_pt)(void *data);
struct ngx_pool_cleanup_s {
	ngx_pool_cleanup_pt   handler;  //回调函数
	void                 *data;      //我们要清理的数据
	ngx_pool_cleanup_t   *next;      //指向下一个清除结构体
};




class NgxMemPool
{
public:
	NgxMemPool(size_t size);
	~NgxMemPool();
	//创建ngx的内存池
	void ngx_create_pool(size_t size);
	//销毁ngx内存池
	void ngx_destroy_pool();
	//重置内存池
	void ngx_reset_pool();
	//开辟内存，对齐
	void* ngx_palloc(size_t size);
	//开辟内存，不对齐
	void* ngx_pnalloc(size_t size);
	//把内存归还给内存池
	void ngx_pfree(void *p);
	//设置内存内存池 
	void set_pool(ngx_pool_t *pool);
	//内存对齐 
	//小内存分配 
	void* ngx_palloc_small(size_t size, ngx_uint_t aligin);
	//大内存分配
	void* ngx_palloc_large(size_t size);
	//内存块大小不够，使用该函数再分配

	void *ngx_memaligin(size_t size);  //alloc底层其实就是对GIibc的malloc进行了封装，传入size即可动态申请内存
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
	cout << "开始析构" << endl;
	ngx_destroy_pool();
	cout << " ~s" << endl;
}
//对齐 
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

//内存不够的时候，额外分配块
void* NgxMemPool::ngx_palloc_block(size_t size)
{
	u_char *m;
	size_t psize;
	ngx_pool_t *p, *New;

	p = _pool;
	psize = (size_t)(p->d.end - (u_char*)_pool);
	m = (u_char*)(this->ngx_memaligin(psize)); //分配和第一块内存大小相同的内存块
	if (m == NULL)
	{
		return NULL;
	}
	cout << "vloc" << endl;
	New = (ngx_pool_t *)m;
	//设置新的end 
	New->d.end = m + psize;
	New->d.next = NULL;
	New->d.failed = 0;

	m += sizeof(ngx_pool_data_t);
	m = ngx_aligin_ptr(m, NGX_ALIGINMENT);
	New->d.last = m + size;

	//看一看当前内存块中的链表节点，查看内存是否够分配，当分配次数到第五次的时候，则去下一块内存块寻址
	for (p = _pool->current; p->d.next;p= p->d.next)
	{
		if (p->d.failed++>4)
		{
			_pool->current = p->d.next;
		}
	}

	
	p->d.next = New;  //将该节点加入到链表中
	return m;
}
void NgxMemPool::set_pool(ngx_pool_t *pool)
{
	_pool = pool;
}

//创建ngx的内存池
void NgxMemPool::ngx_create_pool(size_t size)
{
	ngx_pool_t *p;

	p = (ngx_pool_t*)ngx_memaligin(sizeof(ngx_pool_t)+size);
	if (p == NULL)
	{
		_pool = NULL;
		return;
	}

	p->d.last = (u_char*)p + sizeof(ngx_pool_t); //last刚开始就是ngx_pool_t的尾部
	p->d.end = (u_char*)p + size;  //指向分配size大小后的内存尾部 
	p->d.next = NULL;
	p->d.failed = 0;

	size = size / 2;  //抄过1/2即为大块内存;
	p->max = (size<NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;  //比较是不是大于最大的4095

	p->large = NULL;
	p->cleanup = NULL;
	p->current = p;
	_pool = p; //将当前对象的内存池指向分配出来的 

	//创建成功
	cout << "success" << endl;
}
//销毁ngx内存池
void  NgxMemPool::ngx_destroy_pool()
{
	ngx_pool_t *p, *n;
	ngx_pool_large_t *l;
	ngx_pool_cleanup_t *c;

	//释放小块内存 
	/*
	for(c=_pool->cleanup;c;c=c->next)
	{
	if(c->handler)
	{
	c->handler(c->data);
	}
	}
	*/
	//释放大块内存 
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
	//释放小块内存 

	printf("%p\n", _pool);
	printf("%p\n", _pool->d.next);
	cout << "s" << endl;


	printf("%p\n", _pool);
	if (!_pool->d.next)  //当前只有一个 
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
	cout << "释放结束" << endl;


}
//重置内存池
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
//开辟内存，对齐
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
//开辟内存，不对齐
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
//把内存归还给内存池
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

//小内存分配 
void* NgxMemPool::ngx_palloc_small(size_t size, ngx_uint_t aligin)
{
	u_char* m;
	ngx_pool_t *p;

	p = _pool->current;
	do{
		m = p->d.last;

		if (aligin)
		{
			m = ngx_aligin_ptr(m, NGX_ALIGINMENT); //地址对齐 
		}

		if ((size_t)(p->d.end - m) >= size)
		{
			p->d.last = m + size;
			cout << "small" << endl;
			return m;
		}

		p = p->d.next;
	} while (p);

	//要申请的内存很大

	cout << "upper than small" << endl;
	return	ngx_palloc_block(size);
}
//大内存分配
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

	//如果找到空闲的large块，如果有则直接把刚刚申请的内存，交给它管理
	for (large = _pool->large; large; large = large->next)
	{
		if (large->alloc == NULL)
		{
			large->alloc = p;
			return p;
		}

		//为了提供效率，如果3次还没有空闲的large块，则创建一个
		if (n++>3)
		{
			break;
		}
	}

	//空闲large块的创建使用的时小块内存分配机制
	large = (ngx_pool_large_s*)ngx_palloc_small(sizeof(ngx_pool_large_t), 1);
	if (large == NULL)
	{
		ngx_free(p);
		return NULL;
	}


	//基本的链表连接 
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