#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <libavstream/lock_free_queue.h>
#include <libavstream/pipeline.hpp>
#include <utility>
#include <fmt/core.h>
#include <random>
#include "Platform/Windows/VisualStudioDebugOutput.h"
VisualStudioDebugOutput d;
using std::mt19937;
typedef std::uniform_int_distribution<int> int_distribution_type;
typedef std::uniform_int_distribution<int> int_distribution_type;
int_distribution_type distribution1000(1,1000);
int_distribution_type distribution10(0,10);
mt19937 generator;

using namespace std;
TEST_CASE( "Lock-Free Queue", "[lfq]" ) {
    vector<vector<uint8_t>> blocks;
    vector<vector<uint8_t>> test_blocks;
    avs::LockFreeQueue lfq;
    atomic<bool> running=false;
    lfq.configure(19200,"Lock Free Queue");
    uint8_t n=0;
    unsigned time_seed=unsigned(chrono::system_clock::now().time_since_epoch().count()%0xFFFFFFFF);
   // generator.seed(time_seed);
    for(size_t i=0;i<1000;i++)
    {
	    int r=distribution1000(generator);
        vector<uint8_t> b(r);
        for(size_t j=0;j<b.size();j++)
        {
            b[j]=n++;
        }
        blocks.push_back(std::move(b));
    }
    auto producerThread = new std::thread([&blocks,&lfq]() {
        avs::Pipeline pipeline;
        pipeline.add(&lfq);
	    auto wait=std::chrono::microseconds((int)pow(10,distribution10(generator)%4));
        for(size_t i=0;i<blocks.size();i++){
            size_t written=0;
            for(size_t j=0;j<3;j++)
            {
                if(lfq.write(&lfq,blocks[i].data(),blocks[i].size(),written))
                    break;
                // try again if failed.
                AVSLOG(Warning)<<"Retry write: "<<j<<"\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            } 
            std::this_thread::sleep_for(wait);
        }
        std::this_thread::yield();
    });
    running=true;
    auto consumerThread = new std::thread([&test_blocks,&blocks,&lfq,&running]() {
        avs::Pipeline pipeline;
        pipeline.add(&lfq);
	    auto wait=std::chrono::microseconds((int)pow(10,distribution10(generator)%4));
        while(running||lfq.blockCount>0){
            size_t bufferSize=0,bytesRead=0;
            lfq.read(&lfq,nullptr,bufferSize,bytesRead);
            if(!bufferSize)
                continue;
            vector<uint8_t> new_block(bufferSize);
            lfq.read(&lfq,new_block.data(),bufferSize,bytesRead);
           /* if(new_block!=blocks[test_blocks.size()]){
	            ::DebugBreak();
            }*/
            test_blocks.push_back(std::move(new_block));
            if(!running)
                continue;
            std::this_thread::sleep_for(wait);
        };
    });
    producerThread->join();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    running=false;
    consumerThread->join();
    for(int i=0;i<test_blocks.size()&&i<blocks.size();i++)
    {
        string str=fmt::format("Test block {0}",i);
        INFO( str );
        REQUIRE(test_blocks[i]==blocks[i]);
        
    }
    REQUIRE(test_blocks.size()==blocks.size());
}