#include <iostream>
#include <functional>
#include <setjmp.h>
#include <deque>

#define TRY ExceptionHandler { []()

#define CATCH(x) , []() { return x; }, []()

#define TRY_END };

#define THROW(x) current_handler->Throw(x);

#define VAL(x) current_handler->PushDtor(x);

enum ExceptionType {
    All, BadFile, OutOfMemory, InvalidArgument
};

class ExceptionHandler;

static thread_local ExceptionHandler* current_handler;

class ExceptionHandler {
public:
    ExceptionHandler(std::initializer_list<std::function<int()>> il) : exception_(0) {
        prev_handler_ = current_handler;
        current_handler = this;

        std::function<int()> func = *il.begin();
        for (auto it = il.begin() + 1; it != il.end(); it += 2) {
            catch_handlers_[(*it)()] = *(it + 1);
        }

        exception_ = setjmp(env_);
        if (exception_ != 0) {
            if (catch_handlers_.find(exception_) != catch_handlers_.end()) {
                catch_handlers_[exception_]();
            } else if (catch_handlers_.find(All) != catch_handlers_.end()) {
                catch_handlers_[All]();
            } else {
                current_handler = prev_handler_;
                THROW(exception_)
            }
            return;
        }

        func();
    }

    template <typename T>
    void PushDtor(T& object) {
        dtors_.push_front([&]() {
            object.~T();
        });
    }

    void Throw(int exception) {
        for (auto& dtor : dtors_) {
            dtor();
        }

        longjmp(env_, exception);
    }

public:
    jmp_buf env_;
    ExceptionHandler* prev_handler_;
    int exception_;

    std::unordered_map<int, std::function<int()>> catch_handlers_;
    std::deque<std::function<void()>> dtors_;
};

struct A {
    A(int a) : a_(a) {
        std::cout << "A " << a_ << std::endl;
    }
    ~A() {
        std::cout << "~A " << a_ << std::endl;
    }
    int a_;
};

int main() {
    TRY {
            std::cout << "1 try" << std::endl;

            A a(1);
            VAL(a)

            //THROW(OUT_OF_MEMORY)

            TRY {
                    std::cout << "2 try" << std::endl;

                    TRY {
                            std::cout << "3 try" << std::endl;
                            A a(2);
                            VAL(a)
                            A b(3);
                            VAL(b)

                            THROW(OutOfMemory)
                            return 0;
                        } CATCH (BadFile) {
                            std::cout << "3 bad file" << std::endl;
                            return 0;
                        } TRY_END
                    return 0;
                } CATCH (BadFile) {
                    std::cout << "2 bad file" << std::endl;
                    return 0;
                } TRY_END

            std::cout << "1 try end" << std::endl;

            return 0;
        } CATCH (BadFile) {
            std::cout << "1 bad file" << std::endl;
            return 0;
        } CATCH (OutOfMemory) {
            std::cout << "1 out of mem" << std::endl;
            return 0;
        } CATCH (All) {
            std::cout << "1 other" << std::endl;
            return 0;
        } TRY_END

    return 0;
}