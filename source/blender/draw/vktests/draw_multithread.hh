/* SPDX-License-Identifier: Apache-2.0 */
#ifdef DRAW_GTEST_SUITE
#define DRAW_TESTING_MT 1
#include "draw_testing.hh"
#endif

#include "FN_field.hh"



#define MEASURE_TIME_INIT const double init_time = PIL_check_seconds_timer();
#define MEASURE_TIME(NAME)  { \
 const double elapsed_time = PIL_check_seconds_timer() - init_time; \
  std::cout << "    ############PERF##########    elapsed  " << elapsed_time << "  s.    ##############  " << NAME <<  std::endl;\
}
#define _INDEX_RANGE 128
#define _DELAYED_MS 2
#define ITEMS_NUM 10000





namespace blender::draw {
    /* *** Parallel iterations over range of integer values. *** */

    static void task_range_iter_func(void* userdata, int index, const TaskParallelTLS* __restrict tls)
    {
        int* data = (int*)userdata;
        data[index] = index;
        *((int*)tls->userdata_chunk) += index;
        //  printf("%d, %d, %d\n", index, data[index], *((int *)tls->userdata_chunk));
    }
    static void task_range_iter_reduce_func(const void* __restrict /*userdata*/,
        void* __restrict join_v,
        void* __restrict userdata_chunk)
    {
        int* join = (int*)join_v;
        int* chunk = (int*)userdata_chunk;
        *join += *chunk;
        //  printf("%d, %d\n", data[ITEMS_NUM], *((int *)userdata_chunk));
    }
    /* *** Parallel iterations over mempool items. *** */

    static void task_mempool_iter_func(void* userdata,
        MempoolIterData* item,
        const TaskParallelTLS* __restrict /*tls*/)
    {
        int* data = (int*)item;
        int* count = (int*)userdata;

        EXPECT_TRUE(data != nullptr);

        *data += 1;
        atomic_sub_and_fetch_uint32((uint32_t*)count, 1);
    }



#ifndef DRAW_GTEST_SUITE
    void GPUTest::test_RangeIter()
#else
    TEST(task, RangeIter)
#endif
    {
        int data[ITEMS_NUM] = { 0 };
        int sum = 0;

        BLI_threadapi_init();
        BLI_task_scheduler_init();

        TaskParallelSettings settings;
        BLI_parallel_range_settings_defaults(&settings);
        settings.min_iter_per_thread = 1;

        settings.userdata_chunk = &sum;
        settings.userdata_chunk_size = sizeof(sum);
        settings.func_reduce = task_range_iter_reduce_func;

        BLI_task_parallel_range(0, ITEMS_NUM, data, task_range_iter_func, &settings);

        /* Those checks should ensure us all items of the listbase were processed once, and only once
         * as expected. */

        int expected_sum = 0;
        for (int i = 0; i < ITEMS_NUM; i++) {
            EXPECT_EQ(data[i], i);
            expected_sum += i;
        }
        EXPECT_EQ(sum, expected_sum);

        BLI_threadapi_exit();
    }


#ifndef DRAW_GTEST_SUITE
    void GPUTest::test_MempoolIter()
#else
    TEST(task, MempoolIter)
#endif
    {
        int* data[ITEMS_NUM];
        BLI_threadapi_init();
        BLI_task_scheduler_init();

        BLI_mempool* mempool = BLI_mempool_create(
            sizeof(*data[0]), ITEMS_NUM, 32, BLI_MEMPOOL_ALLOW_ITER);

        int i;

        /* 'Randomly' add and remove some items from mempool, to create a non-homogeneous one. */
        int items_num = 0;
        for (i = 0; i < ITEMS_NUM; i++) {
            data[i] = (int*)BLI_mempool_alloc(mempool);
            *data[i] = i - 1;
            items_num++;
        }

        for (i = 0; i < ITEMS_NUM; i += 3) {
            BLI_mempool_free(mempool, data[i]);
            data[i] = nullptr;
            items_num--;
        }

        for (i = 0; i < ITEMS_NUM; i += 7) {
            if (data[i] == nullptr) {
                data[i] = (int*)BLI_mempool_alloc(mempool);
                *data[i] = i - 1;
                items_num++;
            }
        }

        for (i = 0; i < ITEMS_NUM - 5; i += 23) {
            for (int j = 0; j < 5; j++) {
                if (data[i + j] != nullptr) {
                    BLI_mempool_free(mempool, data[i + j]);
                    data[i + j] = nullptr;
                    items_num--;
                }
            }
        }

        TaskParallelSettings settings;
        BLI_parallel_mempool_settings_defaults(&settings);

        BLI_task_parallel_mempool(mempool, &items_num, task_mempool_iter_func, &settings);

        /* Those checks should ensure us all items of the mempool were processed once, and only once - as
         * expected. */
        EXPECT_EQ(items_num, 0);
        for (i = 0; i < ITEMS_NUM; i++) {
            if (data[i] != nullptr) {
                EXPECT_EQ(*data[i], i);
            }
        }

        BLI_mempool_destroy(mempool);
        BLI_threadapi_exit();
    }



    /* *** Parallel iterations over double-linked list items. *** */

    static void task_listbase_iter_func(void* userdata,
        void* item,
        int index,
        const TaskParallelTLS* __restrict /*tls*/)
    {
        LinkData* data = (LinkData*)item;
        int* count = (int*)userdata;

        data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + index);
        atomic_sub_and_fetch_uint32((uint32_t*)count, 1);
    }



#ifndef DRAW_GTEST_SUITE
    void GPUTest::test_ListBaseIter()
#else
    TEST(task, ListBaseIter)
#endif
    {
        ListBase list = { nullptr, nullptr };
        LinkData* items_buffer = (LinkData*)MEM_calloc_arrayN(
            ITEMS_NUM, sizeof(*items_buffer), __func__);
        BLI_threadapi_init();
        BLI_task_scheduler_init();

        int i;

        int items_num = 0;
        for (i = 0; i < ITEMS_NUM; i++) {
            BLI_addtail(&list, &items_buffer[i]);
            items_num++;
        }

        TaskParallelSettings settings;
        BLI_parallel_range_settings_defaults(&settings);

        BLI_task_parallel_listbase(&list, &items_num, task_listbase_iter_func, &settings);

        /* Those checks should ensure us all items of the listbase were processed once, and only once -
         * as expected. */
        EXPECT_EQ(items_num, 0);
        LinkData* item;
        for (i = 0, item = (LinkData*)list.first; i < ITEMS_NUM && item != nullptr;
            i++, item = item->next) {
            EXPECT_EQ(POINTER_AS_INT(item->data), i);
        }
        EXPECT_EQ(ITEMS_NUM, i);

        MEM_freeN(items_buffer);
        BLI_threadapi_exit();
    }







#ifndef DRAW_GTEST_SUITE
    void GPUTest::test_ParallelInvoke()
#else
    TEST(task, ParallelInvoke)
#endif
 {
        BLI_threadapi_init();
        BLI_task_scheduler_init();


        std::atomic<int> counter = 0;

        struct Item {
            int idx;
            int value;
        };
        typedef Map<unsigned int, Item> Mapty;
        Array<Item> atomap(16);

        counter = 0;
        /* Find the root for element. With multi-threading, this root is not deterministic. So
        * some postprocessing has to be done to make it deterministic. */


        std::atomic<int> index_ = 0;
        threading::EnumerableThreadSpecific<Mapty> first_occurrence_by_root_per_thread;
        std::chrono::milliseconds dura(_DELAYED_MS);
   
        {
            MEASURE_TIME_INIT
            for (int i = 0; i < _INDEX_RANGE; i++) {
                std::this_thread::sleep_for(dura);
             }
            MEASURE_TIME("nothreads")
        
        }

        {
            MEASURE_TIME_INIT

                threading::parallel_for(IndexRange(_INDEX_RANGE), 2, [&](const IndexRange range) {
                Mapty& locmap = first_occurrence_by_root_per_thread.local();
                for (const int i : range) {
                    std::thread::id tid = std::this_thread::get_id();
                    counter++;
                    unsigned int Tid = *static_cast<unsigned int*>(static_cast<void*>(&tid));

                    locmap.add_or_modify(
                        Tid,
                        [&](Item* item) {
                            item->idx = index_.fetch_add(1);// atomap.push(Tid, 1);
                            item->value = 1;
                            atomap[item->idx] = { (int)Tid,1 };
                            //std::cout << "thread " << Tid << "     atomap index " << item->idx << std::endl;
                        },
                        [&](Item* item) {
                            item->value++;
                            atomap[item->idx] = { (int)Tid, item->value };
                        });

                    std::this_thread::sleep_for(dura);

                    //auto val = locmap.lookup(Tid);
                   //std::cout << "thread " << tid <<   "     atomap index " << val.idx << "     counter " << val.value << std::endl;
                }
                 });

            MEASURE_TIME("parallel_for")
        }

        EXPECT_EQ(counter, _INDEX_RANGE);
        counter = 0;
        for (int i= 0;i<16;i++){
            auto item = atomap[i];
            std::cout << "                                            Thread  " << item.idx  << "      val=   " << item.value  << std::endl;
            counter += item.value;
        }

        EXPECT_EQ(counter, _INDEX_RANGE);
   

        BLI_threadapi_exit();
    }



#ifndef DRAW_GTEST_SUITE
    void GPUTest::test_Task()
#else
    TEST(task, Task)
#endif
    {



        BLI_threadapi_init();
        BLI_task_scheduler_init();

        
        struct Udata {
            int index;
        }udata;
        struct Task {
            struct RNG* rng = nullptr, * rng_path = nullptr;
            int begin = 0, end = 0;
            unsigned int tid;
        } tasks[_INDEX_RANGE];

        auto  exec_child_path_cache = [](TaskPool* __restrict pool, void* taskdata)
        {

                std::chrono::milliseconds dura(_DELAYED_MS);
                std::thread::id tid = std::this_thread::get_id();
                unsigned int Tid = *static_cast<unsigned int*>(static_cast<void*>(&tid));
                Task* task = (Task*)taskdata;
                task->tid = Tid;
                //auto val = locmap.lookup(Tid);
               //std::cout << "thread " << tid <<   "     atomap index " << val.idx << "     counter " << val.value << std::endl;
                std::this_thread::sleep_for(dura);
        };

        {
            std::chrono::milliseconds dura(_DELAYED_MS);
            MEASURE_TIME_INIT
                for (int i = 0; i < _INDEX_RANGE; i++) {
                    std::this_thread::sleep_for(dura);
                }
            MEASURE_TIME("nothreads")

        }

        {
                MEASURE_TIME_INIT
                TaskPool* task_pool = BLI_task_pool_create(&udata, TASK_PRIORITY_HIGH);

                for (auto& task : tasks) {
                BLI_task_pool_push(task_pool, exec_child_path_cache, (void*)&task, false, NULL);
                }

                BLI_task_pool_work_and_wait(task_pool);
                BLI_task_pool_free(task_pool);
                MEASURE_TIME("taskpool")

        }

        Map<unsigned int, int> count;
        for (auto& task : tasks) {
            count.add_or_modify(
                task.tid,
                [&](int* item) {
                    *item = 1;
                },
                [&](int* item) {
                    (*item)++;
                });
        };


        int counter = 0;

        for (auto [k,v] : count.items() ) {
            std::cout << "                                                                             Thread  " << k <<   "     Count   "<<v << std::endl;
            counter += v;
        };

        EXPECT_EQ(counter, _INDEX_RANGE);


        BLI_threadapi_exit();



    };

};