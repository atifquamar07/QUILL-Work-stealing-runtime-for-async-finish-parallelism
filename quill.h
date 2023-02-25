#include <bits/stdc++.h>
#include <cstring>
#include <functional>
#include <stdlib.h>

int num_workers = 1;

class quill {

    public:

        //intialize runtime variables
        static void init_runtime();

        //accepts a C++11 lambda function
        static void async(std::function<void()> &&lambda); 

        static void start_finish();

        static void end_finish();

        static void finalize_runtime();

};

