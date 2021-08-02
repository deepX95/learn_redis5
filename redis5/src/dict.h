/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)

// Hash表节点
typedef struct dictEntry {
    void *key;  // 存储键
    union {
        void *val;  // db.dict中的val
        uint64_t u64;
        int64_t s64;  // db.expires中存储过期时间s
        double d;
    } v;  // 值，是个联合体
    struct dictEntry *next;  // 当Hash冲突时，指向冲突的元素，形成单链表
} dictEntry;

typedef struct dictType {
    // 当前运行的迭代器数
    uint64_t (*hashFunction)(const void *key);

    // 键对应的复制函数
    void *(*keyDup)(void *privdata, const void *key);

    // 值对应的复制函数
    void *(*valDup)(void *privdata, const void *obj);

    // 键的比对函数
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);

    // 键的销毁函数
    void (*keyDestructor)(void *privdata, void *key);

    // 值的销毁函数
    void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
// Hash表
typedef struct dictht {
    dictEntry **table;  // 二维数组，哈希表节点指针数组（俗称桶，bucket） 用于存储键值对
    unsigned long size;  // Hash表大小
    unsigned long sizemask; // 长度掩码=size-1，用于计算索引值
    unsigned long used;  // table数组已存元素个数，包含next单链表的数据
} dictht;

// 字典
typedef struct dict {
    dictType *type;  // 该字典对应的特定操作函数，为了实现各种形态的字典而抽象出来的一组操作函数
    void *privdata;  // 该字典依赖的数据，配合type字段指向的函数一起使用
    dictht ht[2];  // 两个Hash表，交替使用，用于rehash操作
    long rehashidx;  // Hash表是否在进行rehash的标识，-1表示没有进行rehash；存储的值表示Hash表ht[0]的rehash操作进行到了哪个索引值
    unsigned long iterators;  // 当前运行的迭代器数，当有安全迭代器绑定到该字典时，会暂停rehash操作
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */
typedef struct dictIterator {
    dict *d;
    long index;
    int table, safe;
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);

typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

#define dictSetSignedIntegerVal(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

#define dictSetDoubleVal(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)

#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

#define dictHashKey(d, key) (d)->type->hashFunction(key)
#define dictGetKey(he) ((he)->key)
#define dictGetVal(he) ((he)->v.val)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
#define dictGetDoubleVal(he) ((he)->v.d)
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/* API */
dict *dictCreate(dictType *type, void *privDataPtr);  // 创建一个新的Hash表

int dictExpand(dict *d, unsigned long size);  // 字典扩容

int dictAdd(dict *d, void *key, void *val);  // 添加键值对，已存在则不添加

dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing);  // 添加key，并返回新添加的key对应的节点。若已存在，则存入existing字段，并返回-1

dictEntry *dictAddOrFind(dict *d, void *key);  // 添加或查找key

int dictReplace(dict *d, void *key, void *val);  // 添加键值对，若存在则修改，否则添加

int dictDelete(dict *d, const void *key);  // 删除节点

dictEntry *dictUnlink(dict *ht, const void *key);  // 删除key,但不释放内存

void dictFreeUnlinkedEntry(dict *d, dictEntry *he);  // 释放dictUnlink函数删除key的内存

void dictRelease(dict *d);  // 释放字典

dictEntry *dictFind(dict *d, const void *key);  // 根据键查找元素

void *dictFetchValue(dict *d, const void *key);  // 根据键查找出值

int dictResize(dict *d);  // 将字典表的大小调整为包含所有元素的最小值，即收缩字典

dictIterator *dictGetIterator(dict *d);  // 初始化普通迭代器

dictIterator *dictGetSafeIterator(dict *d);  // 初始化安全迭代器

dictEntry *dictNext(dictIterator *iter);  // 通过迭代器获取下一个节点

void dictReleaseIterator(dictIterator *iter);  // 释放迭代器

dictEntry *dictGetRandomKey(dict *d);  // 随机得到一个键

unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);  //

void dictGetStats(char *buf, size_t bufsize, dict *d);  // 读取字典的状态、使用情况等

uint64_t dictGenHashFunction(const void *key, int len);  // hash函数-字母大小写敏感

uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len);  // Hash函数，字母大小写不敏感

void dictEmpty(dict *d, void(callback)(void *));  // 清空一个字典

void dictEnableResize(void);  // 开启Resize

void dictDisableResize(void);  // 关闭Resize

int dictRehash(dict *d, int n);  // 渐进式rehash，n为进行几步

int dictRehashMilliseconds(dict *d, int ms);  // 持续性rehash，ms为持续多久

void dictSetHashFunctionSeed(uint8_t *seed);  // 设置新的散列种子

uint8_t *dictGetHashFunctionSeed(void);  // 获取当前散列种子值

unsigned long
dictScan(dict *d, unsigned long v, dictScanFunction *fn, dictScanBucketFunction *bucketfn,
         void *privdata);  // 间断性的迭代字段数据

uint64_t dictGetHash(dict *d, const void *key);  // 得到键的Hash值

dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash);  // 使用指针+hash值去查找元素

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
