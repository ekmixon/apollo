#ifndef APOLLO_MODEL_H
#define APOLLO_MODEL_H

#include <string>

#include "external/nlohmann/json.hpp"
using json = nlohmann::json;

#include "apollo/Apollo.h"


#define APOLLO_DEFAULT_MODEL_TYPE   Apollo::Model::Type::Random

class Apollo::Model {
    public:
        // Forward declarations of model types:
        class Random       ; // : public ModelObject;
        class Sequential   ; // : public ModelObject;
        class Static       ; // : public ModelObject;
        class RoundRobin   ; // : public ModelObject;
        class DecisionTree ; // : public ModelObject;

        class Type {
            public:
                static constexpr int Default      = 0;
                static constexpr int Random       = 1;
                static constexpr int Sequential   = 2;
                static constexpr int Static       = 3;
                static constexpr int RoundRobin   = 4;
                static constexpr int DecisionTree = 5;

                // Default configuration JSON:
                const char *DefaultConfigJSON =  "\n"                                       \
                                                 "{\n"                                      \
                                                 "    \"driver\": {\n"                      \
                                                 "        \"format\": \"int\",\n"           \
                                                 "        \"rules\": {\n"                   \
                                                 "            \"__ANY_REGION__\": \"1\"\n"  \
                                                 "        },\n"                             \
                                                 "    },\n"                                 \
                                                 "    \"type\": {\n"                        \
                                                 "        \"guid\": 0,\n"                   \
                                                 "        \"name\": \"RoundRobin\"\n"       \
                                                 "    },\n"                                 \
                                                 "    \"region_names\": [\n"                \
                                                 "         \"__ANY_REGION__\"\n"            \
                                                 "    ],\n"                                 \
                                                 "    \"features\": {\n"                    \
                                                 "        \"count\": 0,\n"                  \
                                                 "        \"names\": [\n"                   \
                                                 "            \"none\"\n"                   \
                                                 "        ]\n"                              \
                                                 "    }\n"                                  \
                                                 "}\n";
        }; //end: class Model::Type

}; //end: class Model

// Abstract
class Apollo::ModelObject {
    public:
        // pure virtual function (establishes this as abstract class)
        virtual void configure(int num_policies, json model_definition) = 0;
        //
        virtual int      getIndex(void) = 0;

        void     setGuid(uint64_t ng) { guid = ng; return;}
        uint64_t getGuid(void)        { return guid; }

        std::string      name           = "";
        bool             training       = false;

    protected:
        Apollo      *apollo;
        //
        bool         configured = false;
        //
        uint64_t     guid;
        int          policy_count;
        json         model_def;
        int          iter_count;

}; //end: Apollo::ModelObject (abstract class)


#endif
