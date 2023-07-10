#include<functional>
#include<cstring>

namespace quill
{
    void init_runtime();

    void finalize_runtime();

    void async(std::function<void()> &&lambda); //accepts a C++11 lambda function 

    void start_finish();

    void end_finish();

} 