#ifndef APOLLO_MODEL_H
#define APOLLO_MODEL_H

#include <string>
#include "apollo/Apollo.h"

#define APOLLO_DEFAULT_MODEL_TYPE   Apollo::Model::Type::Random

class Apollo::Model {
    public:
        // Forward declarations of model types:
        class Random       ; // : public ModelObject;
        class Sequential   ; // : public ModelObject;
        class Static       ; // : public ModelObject;
        class DecisionTree ; // : public ModelObject;
        class Python       ; // : public ModelObject;

        class Type {
            public:
                static constexpr int Default      = 0;
                static constexpr int Random       = 1;
                static constexpr int Sequential   = 2;
                static constexpr int Static       = 3;
                static constexpr int DecisionTree = 4;
                static constexpr int Python       = 5;

                // Default configuration JSON:
                const char *DefaultConfigJSON =  "\n"                                    \
                                                 "{\n"                                   \
                                                 "    \"driver\": {\n"                   \
                                                 "        \"format\": \"int\",\n"        \
                                                 "        \"rules\": \"0\"\n"            \
                                                 "    },\n"                              \
                                                 "    \"type\": {\n"                     \
                                                 "        \"index\": 3,\n"               \
                                                 "        \"name\": \"Static\"\n"        \
                                                 "    },\n"                              \
                                                 "    \"region_names\": [\n"             \
                                                 "         \"none\"\n"                   \
                                                 "    ],\n"                              \
                                                 "    \"features\": {\n"                 \
                                                 "        \"count\": 0,\n"               \
                                                 "        \"names\": [\n"                \
                                                 "            \"none\"\n"                \
                                                 "        ]\n"                           \
                                                 "    }\n"                               \
                                                 "}\n";
        }; //end: class Model::Type

}; //end: class Model

// Abstract
class Apollo::ModelObject {
    public:
        // pure virtual function (establishes this as abstract class)
        virtual void configure(
                Apollo     *apollo_ptr,
                int         num_policies,
                std::string model_definition) = 0;
        //
        virtual int  getIndex(void) = 0;

    protected:
        Apollo      *apollo;
        //
        bool         configured = false;
        //
        uint64_t     id;
        int          policy_count;
        std::string  model_def;
        int          iter_count;

}; //end: Apollo::ModelObject (abstract class)


#endif
