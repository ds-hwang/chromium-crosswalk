// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataConsumerHandleTestUtil_h
#define DataConsumerHandleTestUtil_h

#include "bindings/core/v8/ScriptState.h"
#include "core/testing/NullExecutionContext.h"
#include "gin/public/isolate_holder.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "modules/fetch/FetchDataLoader.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Deque.h"
#include "wtf/Locker.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <v8.h>

namespace blink {

class DataConsumerHandleTestUtil {
    STATIC_ONLY(DataConsumerHandleTestUtil);
public:
    class NoopClient final : public WebDataConsumerHandle::Client {
        DISALLOW_NEW();
    public:
        void didGetReadable() override { }
    };

    // Thread has a WebThreadSupportingGC. It initializes / shutdowns
    // additional objects based on the given policy. The constructor and the
    // destructor blocks during the setup and the teardown.
    class Thread final {
        USING_FAST_MALLOC(Thread);
    public:
        // Initialization policy of a thread.
        enum InitializationPolicy {
            // Only garbage collection is supported.
            GarbageCollection,
            // Creating an isolate in addition to GarbageCollection.
            ScriptExecution,
            // Creating an execution context in addition to ScriptExecution.
            WithExecutionContext,
        };

        Thread(const char* name, InitializationPolicy = GarbageCollection);
        ~Thread();

        WebThreadSupportingGC* thread() { return m_thread.get(); }
        ExecutionContext* executionContext() { return m_executionContext.get(); }
        ScriptState* scriptState() { return m_scriptState.get(); }
        v8::Isolate* isolate() { return m_isolateHolder->isolate(); }

    private:
        void initialize();
        void shutdown();

        OwnPtr<WebThreadSupportingGC> m_thread;
        const InitializationPolicy m_initializationPolicy;
        OwnPtr<WaitableEvent> m_waitableEvent;
        RefPtrWillBePersistent<NullExecutionContext> m_executionContext;
        OwnPtr<gin::IsolateHolder> m_isolateHolder;
        RefPtr<ScriptState> m_scriptState;
    };

    class ThreadingTestBase : public ThreadSafeRefCounted<ThreadingTestBase> {
    public:
        virtual ~ThreadingTestBase() { }

        class ThreadHolder;

        class Context : public ThreadSafeRefCounted<Context> {
        public:
            static PassRefPtr<Context> create() { return adoptRef(new Context); }
            void recordAttach(const String& handle)
            {
                MutexLocker locker(m_loggingMutex);
                m_result.append("A reader is attached to " + handle + " on " + currentThreadName() + ".\n");
            }
            void recordDetach(const String& handle)
            {
                MutexLocker locker(m_loggingMutex);
                m_result.append("A reader is detached from " + handle + " on " + currentThreadName() + ".\n");
            }

            const String& result()
            {
                MutexLocker locker(m_loggingMutex);
                return m_result;
            }

            void registerThreadHolder(ThreadHolder* holder)
            {
                MutexLocker locker(m_holderMutex);
                ASSERT(!m_holder);
                m_holder = holder;
            }
            void unregisterThreadHolder()
            {
                MutexLocker locker(m_holderMutex);
                ASSERT(m_holder);
                m_holder = nullptr;
            }
            void postTaskToReadingThread(const WebTraceLocation& location, PassOwnPtr<Closure> task)
            {
                MutexLocker locker(m_holderMutex);
                ASSERT(m_holder);
                m_holder->readingThread()->postTask(location, task);
            }
            void postTaskToUpdatingThread(const WebTraceLocation& location, PassOwnPtr<Closure> task)
            {
                MutexLocker locker(m_holderMutex);
                ASSERT(m_holder);
                m_holder->updatingThread()->postTask(location, task);
            }

        private:
            Context()
                : m_holder(nullptr) { }
            String currentThreadName()
            {
                MutexLocker locker(m_holderMutex);
                if (m_holder) {
                    if (m_holder->readingThread()->isCurrentThread())
                        return "the reading thread";
                    if (m_holder->updatingThread()->isCurrentThread())
                        return "the updating thread";
                }
                return "an unknown thread";
            }

            // Protects |m_result|.
            Mutex m_loggingMutex;
            String m_result;

            // Protects |m_holder|.
            Mutex m_holderMutex;
            // Because Context outlives ThreadHolder, holding a raw pointer
            // here is safe.
            ThreadHolder* m_holder;
        };

        // The reading/updating threads are alive while ThreadHolder is alive.
        class ThreadHolder {
            DISALLOW_NEW();
        public:
            ThreadHolder(ThreadingTestBase* test)
                : m_context(test->m_context)
                , m_readingThread(adoptPtr(new Thread("reading thread")))
                , m_updatingThread(adoptPtr(new Thread("updating thread")))
            {
                m_context->registerThreadHolder(this);
            }
            ~ThreadHolder()
            {
                m_context->unregisterThreadHolder();
            }

            WebThreadSupportingGC* readingThread() { return m_readingThread->thread(); }
            WebThreadSupportingGC* updatingThread() { return m_updatingThread->thread(); }

        private:
            RefPtr<Context> m_context;
            OwnPtr<Thread> m_readingThread;
            OwnPtr<Thread> m_updatingThread;
        };

        class ReaderImpl final : public WebDataConsumerHandle::Reader {
            USING_FAST_MALLOC(ReaderImpl);
        public:
            ReaderImpl(const String& name, PassRefPtr<Context> context) : m_name(name.isolatedCopy()), m_context(context)
            {
                m_context->recordAttach(m_name.isolatedCopy());
            }
            ~ReaderImpl() override { m_context->recordDetach(m_name.isolatedCopy()); }

            using Result = WebDataConsumerHandle::Result;
            using Flags = WebDataConsumerHandle::Flags;
            Result beginRead(const void**, Flags, size_t*) override { return WebDataConsumerHandle::ShouldWait; }
            Result endRead(size_t) override { return WebDataConsumerHandle::UnexpectedError; }

        private:
            const String m_name;
            RefPtr<Context> m_context;
        };
        class DataConsumerHandle final : public WebDataConsumerHandle {
            USING_FAST_MALLOC(DataConsumerHandle);
        public:
            static PassOwnPtr<WebDataConsumerHandle> create(const String& name, PassRefPtr<Context> context)
            {
                return adoptPtr(new DataConsumerHandle(name, context));
            }

        private:
            DataConsumerHandle(const String& name, PassRefPtr<Context> context) : m_name(name.isolatedCopy()), m_context(context) { }

            Reader* obtainReaderInternal(Client*) { return new ReaderImpl(m_name, m_context); }
            const char* debugName() const override { return "ThreadingTestBase::DataConsumerHandle"; }

            const String m_name;
            RefPtr<Context> m_context;
        };

        void resetReader() { m_reader = nullptr; }
        void signalDone() { m_waitableEvent->signal(); }
        const String& result() { return m_context->result(); }
        void postTaskToReadingThread(const WebTraceLocation& location, PassOwnPtr<Closure> task)
        {
            m_context->postTaskToReadingThread(location,  task);
        }
        void postTaskToUpdatingThread(const WebTraceLocation& location, PassOwnPtr<Closure> task)
        {
            m_context->postTaskToUpdatingThread(location,  task);
        }
        void postTaskToReadingThreadAndWait(const WebTraceLocation& location, PassOwnPtr<Closure> task)
        {
            postTaskToReadingThread(location,  task);
            m_waitableEvent->wait();
        }
        void postTaskToUpdatingThreadAndWait(const WebTraceLocation& location, PassOwnPtr<Closure> task)
        {
            postTaskToUpdatingThread(location,  task);
            m_waitableEvent->wait();
        }
    protected:
        ThreadingTestBase()
            : m_context(Context::create()) { }

        RefPtr<Context> m_context;
        OwnPtr<WebDataConsumerHandle::Reader> m_reader;
        OwnPtr<WaitableEvent> m_waitableEvent;
        NoopClient m_client;
    };

    class ThreadingHandleNotificationTest : public ThreadingTestBase, public WebDataConsumerHandle::Client {
    public:
        using Self = ThreadingHandleNotificationTest;
        static PassRefPtr<Self> create() { return adoptRef(new Self); }

        void run(PassOwnPtr<WebDataConsumerHandle> handle)
        {
            ThreadHolder holder(this);
            m_waitableEvent = adoptPtr(new WaitableEvent());
            m_handle = handle;

            postTaskToReadingThreadAndWait(BLINK_FROM_HERE, threadSafeBind(&Self::obtainReader, this));
        }

    private:
        ThreadingHandleNotificationTest() = default;
        void obtainReader()
        {
            m_reader = m_handle->obtainReader(this);
        }
        void didGetReadable() override
        {
            postTaskToReadingThread(BLINK_FROM_HERE, threadSafeBind(&Self::resetReader, this));
            postTaskToReadingThread(BLINK_FROM_HERE, threadSafeBind(&Self::signalDone, this));
        }

        OwnPtr<WebDataConsumerHandle> m_handle;
    };

    class ThreadingHandleNoNotificationTest : public ThreadingTestBase, public WebDataConsumerHandle::Client {
    public:
        using Self = ThreadingHandleNoNotificationTest;
        static PassRefPtr<Self> create() { return adoptRef(new Self); }

        void run(PassOwnPtr<WebDataConsumerHandle> handle)
        {
            ThreadHolder holder(this);
            m_waitableEvent = adoptPtr(new WaitableEvent());
            m_handle = handle;

            postTaskToReadingThreadAndWait(BLINK_FROM_HERE, threadSafeBind(&Self::obtainReader, this));
        }

    private:
        ThreadingHandleNoNotificationTest() = default;
        void obtainReader()
        {
            m_reader = m_handle->obtainReader(this);
            m_reader = nullptr;
            postTaskToReadingThread(BLINK_FROM_HERE, threadSafeBind(&Self::signalDone, this));
        }
        void didGetReadable() override
        {
            ASSERT_NOT_REACHED();
        }

        OwnPtr<WebDataConsumerHandle> m_handle;
    };

    class MockFetchDataConsumerHandle : public FetchDataConsumerHandle {
    public:
        static PassOwnPtr<::testing::StrictMock<MockFetchDataConsumerHandle>> create() { return adoptPtr(new ::testing::StrictMock<MockFetchDataConsumerHandle>); }
        MOCK_METHOD1(obtainReaderInternal, Reader*(Client*));

    private:
        const char* debugName() const override { return "MockFetchDataConsumerHandle"; }
    };

    class MockFetchDataConsumerReader : public FetchDataConsumerHandle::Reader {
    public:
        static PassOwnPtr<::testing::StrictMock<MockFetchDataConsumerReader>> create() { return adoptPtr(new ::testing::StrictMock<MockFetchDataConsumerReader>); }

        using Result = WebDataConsumerHandle::Result;
        using Flags = WebDataConsumerHandle::Flags;
        MOCK_METHOD4(read, Result(void*, size_t, Flags, size_t*));
        MOCK_METHOD3(beginRead, Result(const void**, Flags, size_t*));
        MOCK_METHOD1(endRead, Result(size_t));
        MOCK_METHOD1(drainAsBlobDataHandle, PassRefPtr<BlobDataHandle>(BlobSizePolicy));

        ~MockFetchDataConsumerReader() override
        {
            destruct();
        }
        MOCK_METHOD0(destruct, void());
    };

    class MockFetchDataLoaderClient : public GarbageCollectedFinalized<MockFetchDataLoaderClient>, public FetchDataLoader::Client {
        USING_GARBAGE_COLLECTED_MIXIN(MockFetchDataLoaderClient);
    public:
        static ::testing::StrictMock<MockFetchDataLoaderClient>* create() { return new ::testing::StrictMock<MockFetchDataLoaderClient>; }

        DEFINE_INLINE_VIRTUAL_TRACE()
        {
            FetchDataLoader::Client::trace(visitor);
        }

        MOCK_METHOD1(didFetchDataLoadedBlobHandleMock, void(RefPtr<BlobDataHandle>));
        MOCK_METHOD1(didFetchDataLoadedArrayBufferMock, void(RefPtr<DOMArrayBuffer>));
        MOCK_METHOD1(didFetchDataLoadedString, void(const String&));
        MOCK_METHOD0(didFetchDataLoadStream, void());
        MOCK_METHOD0(didFetchDataLoadFailed, void());

        // In mock methods we use RefPtr<> rather than PassRefPtr<>.
        void didFetchDataLoadedArrayBuffer(PassRefPtr<DOMArrayBuffer> arrayBuffer) override
        {
            didFetchDataLoadedArrayBufferMock(arrayBuffer);
        }
        void didFetchDataLoadedBlobHandle(PassRefPtr<BlobDataHandle> blobDataHandle) override
        {
            didFetchDataLoadedBlobHandleMock(blobDataHandle);
        }
    };

    class Command final {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        enum Name {
            Data,
            Done,
            Error,
            Wait,
        };

        Command(Name name) : m_name(name) { }
        Command(Name name, const Vector<char>& body) : m_name(name), m_body(body) { }
        Command(Name name, const char* body, size_t size) : m_name(name)
        {
            m_body.append(body, size);
        }
        Command(Name name, const char* body) : Command(name, body, strlen(body)) { }
        Name name() const { return m_name; }
        const Vector<char>& body() const { return m_body; }

    private:
        const Name m_name;
        Vector<char> m_body;
    };

    // ReplayingHandle stores commands via |add| and replays the stored commends when read.
    class ReplayingHandle final : public WebDataConsumerHandle {
        USING_FAST_MALLOC(ReplayingHandle);
    public:
        static PassOwnPtr<ReplayingHandle> create() { return adoptPtr(new ReplayingHandle()); }
        ~ReplayingHandle();

        // Add a command to this handle. This function must be called on the
        // creator thread. This function must be called BEFORE any reader is
        // obtained.
        void add(const Command&);

        class Context final : public ThreadSafeRefCounted<Context> {
        public:
            static PassRefPtr<Context> create() { return adoptRef(new Context); }

            // This function cannot be called after creating a tee.
            void add(const Command&);
            void attachReader(WebDataConsumerHandle::Client*);
            void detachReader();
            void detachHandle();
            Result beginRead(const void** buffer, Flags, size_t* available);
            Result endRead(size_t readSize);
            WaitableEvent* detached() { return m_detached.get(); }

        private:
            Context();
            bool isEmpty() const { return m_commands.isEmpty(); }
            const Command& top();
            void consume(size_t);
            size_t offset() const { return m_offset; }
            void notify();
            void notifyInternal();

            Deque<Command> m_commands;
            size_t m_offset;
            WebThread* m_readerThread;
            Client* m_client;
            Result m_result;
            bool m_isHandleAttached;
            Mutex m_mutex;
            OwnPtr<WaitableEvent> m_detached;
        };

        Context* context() { return m_context.get(); }

    private:
        class ReaderImpl;

        ReplayingHandle();
        Reader* obtainReaderInternal(Client*) override;
        const char* debugName() const override { return "ReplayingHandle"; }

        RefPtr<Context> m_context;
    };

    class HandleReadResult final {
        USING_FAST_MALLOC(HandleReadResult);
    public:
        HandleReadResult(WebDataConsumerHandle::Result result, const Vector<char>& data) : m_result(result), m_data(data) { }
        WebDataConsumerHandle::Result result() const { return m_result; }
        const Vector<char>& data() const { return m_data; }

    private:
        const WebDataConsumerHandle::Result m_result;
        const Vector<char> m_data;
    };

    // HandleReader reads all data from the given WebDataConsumerHandle using
    // Reader::read on the thread on which it is created. When reading is done
    // or failed, it calls the given callback with the result.
    class HandleReader final : public WebDataConsumerHandle::Client {
        USING_FAST_MALLOC(HandleReader);
    public:
        using OnFinishedReading = WTF::Function<void(PassOwnPtr<HandleReadResult>)>;

        HandleReader(PassOwnPtr<WebDataConsumerHandle>, PassOwnPtr<OnFinishedReading>);
        void didGetReadable() override;

    private:
        void runOnFinishedReading(PassOwnPtr<HandleReadResult>);

        OwnPtr<WebDataConsumerHandle::Reader> m_reader;
        OwnPtr<OnFinishedReading> m_onFinishedReading;
        Vector<char> m_data;
    };

    // HandleTwoPhaseReader does the same as HandleReader, but it uses
    // |beginRead| / |endRead| instead of |read|.
    class HandleTwoPhaseReader final : public WebDataConsumerHandle::Client {
        USING_FAST_MALLOC(HandleTwoPhaseReader);
    public:
        using OnFinishedReading = WTF::Function<void(PassOwnPtr<HandleReadResult>)>;

        HandleTwoPhaseReader(PassOwnPtr<WebDataConsumerHandle>, PassOwnPtr<OnFinishedReading>);
        void didGetReadable() override;

    private:
        void runOnFinishedReading(PassOwnPtr<HandleReadResult>);

        OwnPtr<WebDataConsumerHandle::Reader> m_reader;
        OwnPtr<OnFinishedReading> m_onFinishedReading;
        Vector<char> m_data;
    };

    // HandleReaderRunner<T> creates a dedicated thread and run T on the thread
    // where T is one of HandleReader and HandleTwophaseReader.
    template <typename T>
    class HandleReaderRunner final {
        STACK_ALLOCATED();
    public:
        explicit HandleReaderRunner(PassOwnPtr<WebDataConsumerHandle> handle)
            : m_thread(adoptPtr(new Thread("reading thread")))
            , m_event(adoptPtr(new WaitableEvent()))
            , m_isDone(false)
        {
            m_thread->thread()->postTask(BLINK_FROM_HERE, threadSafeBind(&HandleReaderRunner::start, AllowCrossThreadAccess(this), handle));
        }
        ~HandleReaderRunner()
        {
            wait();
        }

        PassOwnPtr<HandleReadResult> wait()
        {
            if (m_isDone)
                return nullptr;
            m_event->wait();
            m_isDone = true;
            return m_result.release();
        }

    private:
        void start(PassOwnPtr<WebDataConsumerHandle> handle)
        {
            m_handleReader = adoptPtr(new T(handle, bind<PassOwnPtr<HandleReadResult>>(&HandleReaderRunner::onFinished, this)));
        }

        void onFinished(PassOwnPtr<HandleReadResult> result)
        {
            m_handleReader = nullptr;
            m_result = result;
            m_event->signal();
        }

        OwnPtr<Thread> m_thread;
        OwnPtr<WaitableEvent> m_event;
        OwnPtr<HandleReadResult> m_result;
        bool m_isDone;

        OwnPtr<T> m_handleReader;
    };
};

} // namespace blink

#endif // DataConsumerHandleTestUtil_h
