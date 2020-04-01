
// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory
//
// This file is part of Apollo.
// OCEC-17-092
// All rights reserved.
//
// Apollo is currently developed by Chad Wood, wood67@llnl.gov, with the help
// of many collaborators.
//
// Apollo was originally created by David Beckingsale, david@llnl.gov
//
// For details, see https://github.com/LLNL/apollo.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#include <iostream>
#include <iomanip>
#include <chrono>
#include <memory>
#include <utility>

#include "assert.h"

#include "apollo/Apollo.h"
#include "apollo/Region.h"
#include "apollo/Logging.h"
#include "apollo/ModelFactory.h"

#include <mpi.h>

int
Apollo::Region::getPolicyIndex(void)
{
#if VERBOSE_DEBUG
    if (not currently_inside_region) {
        fprintf(stderr, "== APOLLO: [WARNING] region->getPolicyIndex() called"
                        " while NOT inside the region. Please call"
                        " region->begin() first so the model has values to use"
                        " when selecting a policy. (region->name == %s)\n", name);
        fflush(stderr);
    }
#endif
    assert( currently_inside_region );

    int choice = model->getIndex( apollo->features );
    //ggout
    //if( !model->training ) {
    //    std::cout << "Region " << name << ", model " << model->name \
    //        << " features " << (int)apollo->features[0] \
    //        << " got -> " << choice << std::endl;
    //}
#if 0
    if (choice != current_policy) {
        std::cout << "Change policy " << current_policy << " -> " << choice << " region " 
            << name << " elems: " << (int)apollo->features[0]<< std::endl; //ggout
    } else {
        //std::cout << "No policy change for region " << name << ", policy " << current_policy << std::endl; //gout
    }
#endif
    current_policy = choice;
    //log("getPolicyIndex took ", evaluation_time_total, " seconds.\n");
    return choice;
}


Apollo::Region::Region(
        const int num_features,
        const char  *regionName,
        int          numAvailablePolicies)
{
    apollo = Apollo::instance();
    apollo->num_features = num_features;
    apollo->num_policies = numAvailablePolicies;
    // TODO: uncomment above
    //apollo->num_policies = 14;

    strncpy(name, regionName, sizeof(name)-1 );
    name[ sizeof(name)-1 ] = '\0';

    current_policy            = -1;
    currently_inside_region   = false;

    // TODO: use best_policies to train a model for new region for which there's training data
    size_t pos = Config::APOLLO_INIT_MODEL.find(",");
    std::string model_str = Config::APOLLO_INIT_MODEL.substr(0, pos);
    if( "Static" == model_str ) {
        int policy_choice = std::stoi( Config::APOLLO_INIT_MODEL.substr( pos+1 ) );
        if( policy_choice < 0 || policy_choice >= numAvailablePolicies ) {
            std::cerr << "Invalid policy_choice " << policy_choice << std::endl;
            abort();
        }
        model = ModelFactory::createStatic( apollo->num_policies, policy_choice );
        //std::cout << "Model Static policy " << policy_choice << std::endl;
    }
    else if( "Random" == model_str ) {
        model = ModelFactory::createRandom( apollo->num_policies );
        //std::cout << "Model Random" << std::endl;
    }
    else if( "RoundRobin" == model_str ) {
        model = ModelFactory::createRoundRobin( apollo->num_policies );
        //std::cout << "Model RoundRobin" << std::endl;
    }
    else {
        std::cerr << "Invalid model env var: " + Config::APOLLO_INIT_MODEL << std::endl;
        abort();
    }

    //std::cout << "Insert region " << name << " ptr " << this << std::endl;
    const auto ret = apollo->regions.insert( { name, this } );

    assert( ret.second == true );

    return;
}

Apollo::Region::~Region()
{
    if (currently_inside_region) {
        this->end();
    }

    return;
}


void
Apollo::Region::begin()
{
#if VERBOSE_DEBUG
    if (currently_inside_region) {
        fprintf(stderr, "== APOLLO: [WARNING] region->begin() called"
                        " while already inside the region. Please call"
                        " region->end() first to avoid unintended"
                        " consequences. (region->name == %s)\n",
                        name);
        fflush(stderr);
    }
#endif

    assert( !currently_inside_region );

    currently_inside_region = true;

    // NOTE: Features are tracked globally within the process.
    //       Apollo semantics require that region.begin/end calls happen
    //       from the top-level process thread, they are not encountered
    //       within a parallel code region.
    //
    //       We update this value here in case another region used a different
    //       policy index, in case this value gets looked up between this
    //       call to region.begin and our model being newly evaluated by
    //       region.getPolicyIndex ...
    //

    current_exec_time_begin = std::chrono::steady_clock::now();
    return;
}

void
Apollo::Region::end()
{
    current_exec_time_end = std::chrono::steady_clock::now();

#if VERBOSE_DEBUG
    if (not currently_inside_region) {
        fprintf(stderr, "== APOLLO: [WARNING] region->end() called"
                        " while NOT inside the region. Please call"
                        " region->begin(step) first to avoid unintended"
                        " consequences. (region->name == %s)\n", name);
        fflush(stderr);
    }
#endif

    assert( currently_inside_region );

    currently_inside_region = false;

    double duration = std::chrono::duration<double>(current_exec_time_end - current_exec_time_begin).count();

    // TODO: reduce overhead, move time calculation to reduceBestPolicies?
    // TODO: buckets of features?
    //const int bucket = 10;
    //for(auto &it : apollo->features) {
    //    //std::cout << "feature: " << it;
    //    int idiv = int(it) / bucket;
    //    it = ( idiv + 1 )* bucket;
    //    //std::cout << " -> " << it;
    //    //std::cout << std::endl;
    //}

    auto iter = measures.find( { apollo->features, current_policy } );
    if (iter == measures.end()) {
        iter = measures.insert(std::make_pair( std::make_pair( apollo->features, current_policy ),
                std::move( std::make_unique<Apollo::Region::Measure>(1, duration) )
                ) ).first;
    } else {
        iter->second->exec_count++;
        iter->second->time_total += duration;
    }

    // TODO: re-train from regression model every region execution?
    //
    //std::cout << "=== INSERT MEASURE ===" << std::endl;
    //std::cout << name << ": " << "[ " << apollo->features[0] << " ]: " \
    //    << current_policy << " -> " << iter->second->exec_count \
    //    << ", " << iter->second->time_total << std::endl;
    //std::cout << "~~~~~~~~~~" << std::endl;


    apollo->features.clear();

    return;
}

int 
Apollo::Region::reduceBestPolicies()
{
    //std::cout << "=================================" << std::endl \
        << "Region " << name << " MEASURES "  << std::endl;
    for (auto iter_measure = measures.begin();
            iter_measure != measures.end();   iter_measure++) {

        const std::vector<float>& feature_vector = iter_measure->first.first;
        const int policy_index                   = iter_measure->first.second;
        auto                           &time_set = iter_measure->second;

        // ggout
        //std::cout << "[ " << (int)feature_vector[0] << " ]: " \
            << "policy: " << policy_index \
            << " , count: " << time_set->exec_count \
            << " , total: " << time_set->time_total \
            << " , time_avg: " <<  ( time_set->time_total / time_set->exec_count ) << std::endl;
        assert( time_set->exec_count > 0 );
        double time_avg = ( time_set->time_total / time_set->exec_count );

        auto iter =  best_policies.find( feature_vector );
        if( iter ==  best_policies.end() ) {
            best_policies.insert( { feature_vector, { policy_index, time_avg } } );
        }
        else {
            // Key exists
            if(  best_policies[ feature_vector ].second > time_avg ) {
                best_policies[ feature_vector ] = { policy_index, time_avg };
            }
        }
    }
    //std::cout << ".-" << std::endl;

    return best_policies.size();
}

void
Apollo::Region::packMeasurements(char *buf, int size, MPI_Comm comm) {
    int pos = 0;

    int rank;
    MPI_Comm_rank( comm, &rank );

    for( auto &it : best_policies ) {
        auto &feature_vector = it.first;
        int policy_index = it.second.first;
        double time_avg = it.second.second;

        // rank
        MPI_Pack( &rank, 1, MPI_INT, buf, size, &pos, comm);
        //std::cout << "rank," << rank << " pos: " << pos << std::endl;

        // features
        for (float value : feature_vector ) {
            MPI_Pack( &value, 1, MPI_FLOAT, buf, size, &pos, comm );
            //std::cout << "feature," << value << " pos: " << pos << std::endl;
        }

        // policy index
        MPI_Pack( &policy_index, 1, MPI_INT, buf, size, &pos, comm );
        //std::cout << "policy_index," << policy_index << " pos: " << pos << std::endl;
        // XXX: use 64 bytes fixed for region_name
        // region name
        MPI_Pack( name, 64, MPI_CHAR, buf, size, &pos, comm );
        //std::cout << "region_name," << name << " pos: " << pos << std::endl;
        // average time
        MPI_Pack( &time_avg, 1, MPI_DOUBLE, buf, size, &pos, comm );
        //std::cout << "time_avg," << time_avg << " pos: " << pos << std::endl;
    }

    return;
}


