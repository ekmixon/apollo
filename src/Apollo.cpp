#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <typeinfo>
#include <unordered_map>
#include <algorithm>


#include "CallpathRuntime.h"

#include "external/nlohmann/json.hpp"
using json = nlohmann::json;

#include "apollo/Apollo.h"
#include "apollo/Logging.h"
#include "apollo/Region.h"
#include "apollo/ModelWrapper.h"
//
#include "util/Debug.h"
//
#include "caliper/cali.h"
#include "caliper/Annotation.h"
//
#include "sos.h"
#include "sos_types.h"

SOS_runtime *sos;
SOS_pub     *pub;

typedef cali::Annotation note;

void
handleFeedback(void *sos_context, int msg_type, int msg_size, void *data)
{
    Apollo *apollo = Apollo::instance();

    SOS_msg_header header;
    int   offset = 0;
    char *tree;
    struct ApolloDec;


    switch (msg_type) {
        //
        case SOS_FEEDBACK_TYPE_QUERY:
            log("Query results received. (msg_size == ", msg_size, ")");
            break;
        case SOS_FEEDBACK_TYPE_CACHE:
            log("Cache results received. (msg_size == ", msg_size, ")");
            break;
        //
        case SOS_FEEDBACK_TYPE_PAYLOAD:
            //log("Trigger payload received.  (msg_size == " << msg_size << ")");
            //void *apollo_ref = SOS_reference_get(
            //        (SOS_runtime *)sos_context,
            //        "APOLLO_CONTEXT");

            //NOTE: data may not be a null-terminated string, so we put it into one.
            char *cleanstr = (char *) calloc(msg_size + 1, sizeof(char));
            strncpy(cleanstr, (const char *)data, msg_size);
            call_Apollo_attachModel(apollo, (char *) cleanstr);
            free(cleanstr);
            break;
    }


    return;
}

extern "C" void
call_Apollo_attachModel(void *apollo_ref, const char *def)
{
    Apollo *apollo = (Apollo *) apollo_ref;
    apollo->attachModel(def);
    return;
}

void
Apollo::attachModel(const char *def)
{
    Apollo *apollo = Apollo::instance();

    int  i;
    bool def_has_wildcard_model = false;

    if (def == NULL) {
        log("[ERROR] apollo->attachModel() called with a"
                    " NULL model definition. Doing nothing.");
        return;
    }

    if (strlen(def) < 1) {
        log("[ERROR] apollo->attachModel() called with an"
                    " empty model definition. Doing nothing.");
        return;
    }

    // --------
    // CHAD's little debugging toy...
    static int rank = -1;
    if (rank < 0) {
        const char *rank_str = getenv("SLURM_PROCID");
        if ((rank_str != NULL) && (strlen(rank_str) > 0))
        {
            rank = atoi(rank_str);
        }
    }
    if (rank == 0) {
        std::cout << "-------- new model has arrived --------" << std::endl;
    }
    // --------


    // Extract the list of region names for this "package of models"
    std::vector<std::string> region_names;
    json j = json::parse(std::string(def));
    if (j.find("region_names") != j.end()) {
        region_names = j["region_names"].get<std::vector<std::string>>();
    }

    if (std::find(std::begin(region_names), std::end(region_names),
                "__ANY_REGION__") != std::end(region_names)) {
        def_has_wildcard_model = true;
    }

    // Roll through the regions in this process, and if it this region is
    // in the list of regions with a new model in the package, configure
    // that region's modelwrapper.  (The def* points to the whole package
    // of models, the configure() method will extract the specific model
    // that applies to it)
    for (auto it : regions) {
        Apollo::Region *region = it.second;
        if (def_has_wildcard_model) {
            // Everyone will get either a specific model from the definintion, or
            // the __ANY_REGION__ fallback.
            region->getModel()->configure(def);
        } else {
            if (std::find(std::begin(region_names), std::end(region_names),
                        region->name) != std::end(region_names)) {
                region->getModel()->configure(def);
            }
        }
    };

    //std::cout << "Done attempting to load new model.\n";
    return;
}



Apollo::Apollo()
{
    ynConnectedToSOS = false;

    sos = NULL;
    pub = NULL;

    SOS_init(&sos, SOS_ROLE_CLIENT,
            SOS_RECEIVES_DIRECT_MESSAGES, handleFeedback);

    if (sos == NULL) {
        fprintf(stderr, "== APOLLO: [WARNING] Unable to communicate"
                " with the SOS daemon.\n");
        return;
    }

    SOS_pub_init(sos, &pub, (char *)"APOLLO", SOS_NATURE_SUPPORT_EXEC);
    SOS_reference_set(sos, "APOLLO_PUB", (void *) pub);

    if (pub == NULL) {
        fprintf(stderr, "== APOLLO: [WARNING] Unable to create"
                " publication handle.\n");
        if (sos != NULL) {
            SOS_finalize(sos);
        }
        sos = NULL;
        return;
    }

    log("Reading SLURM env...");
    try {
        numNodes = std::stoi(getenv("SLURM_NNODES"));
        log("    numNodes ................: ", numNodes);

        numProcs = std::stoi(getenv("SLURM_NPROCS"));
        log("    numProcs ................: ", numProcs);

        numCPUsOnNode = std::stoi(getenv("SLURM_CPUS_ON_NODE"));
        log("    numCPUsOnNode ...........: ", numCPUsOnNode);

        std::string envProcPerNode = getenv("SLURM_TASKS_PER_NODE");
        // Sometimes SLURM sets this to something like "4(x2)" and
        // all we care about here is the "4":
        auto pos = envProcPerNode.find('(');
        if (pos != envProcPerNode.npos) {
            numProcsPerNode = std::stoi(envProcPerNode.substr(0, pos));
        } else {
            numProcsPerNode = std::stoi(envProcPerNode);
        }
        log("    numProcsPerNode .........: ", numProcsPerNode);

        numThreadsPerProcCap = std::max(1, (int)(numCPUsOnNode / numProcsPerNode));
        log("    numThreadsPerProcCap ....: ", numThreadsPerProcCap);

    } catch (...) {
        fprintf(stderr, "== APOLLO: [ERROR] Unable to read values from SLURM"
                " environment variables.\n");
        if (sos != NULL) {
            SOS_finalize(sos);
        }
        exit(EXIT_FAILURE);
    }



    // At this point we have a valid SOS runtime and pub handle.
    // NOTE: The assumption here is that there is 1:1 ratio of Apollo
    //       instances per process.
    SOS_reference_set(sos, "APOLLO_CONTEXT", (void *) this);
    SOS_sense_register(sos, "APOLLO_MODELS");

    ynConnectedToSOS = true;

    SOS_guid guid = SOS_uid_next(sos->uid.my_guid_pool);

    note_flush =
        (void *) new note("APOLLO_time_flush", CALI_ATTR_ASVALUE);
    note_time_for_region =
        (void *) new note("region_name", CALI_ATTR_ASVALUE);
    note_time_for_step =
        (void *) new note("step", CALI_ATTR_ASVALUE);
    note_time_exec_count =
        (void *) new note("exec_count", CALI_ATTR_ASVALUE);
    note_time_last =
        (void *) new note("time_last", CALI_ATTR_ASVALUE);
    note_time_min =
        (void *) new note("time_min", CALI_ATTR_ASVALUE);
    note_time_max =
        (void *) new note("time_max", CALI_ATTR_ASVALUE);
    note_time_avg =
        (void *) new note("time_avg", CALI_ATTR_ASVALUE);

    log("Initialized.");

    return;
}

Apollo::~Apollo()
{
    if (sos != NULL) {
        SOS_finalize(sos);
        sos = NULL;
    }
    delete (note *) note_flush;
    delete (note *) note_time_for_region;
    delete (note *) note_time_for_step;
    delete (note *) note_time_exec_count;
    delete (note *) note_time_last;
    delete (note *) note_time_min;
    delete (note *) note_time_max;
    delete (note *) note_time_avg;

}

void
Apollo::flushAllRegionMeasurements(int assign_to_step)
{
    auto it = regions.begin();
    while (it != regions.end()) {
        Apollo::Region *reg = it->second;
        reg->flushMeasurements(assign_to_step);
        ++it;
    }
    note *t_flush = (note *) note_flush;
    t_flush->begin(1);
    t_flush->end();
    return;
}



void
Apollo::setFeature(std::string set_name, double set_value)
{
    bool found = false;

    for (int i = 0; i < features.size(); ++i) {
        if (features[i].name == set_name) {
            found = true;
            features[i].value = set_value;
            break;
        }
    }

    if (not found) {
        Apollo::Feature f;
        f.name  = set_name;
        f.value = set_value;

        features.push_back(std::move(f));

        note *n = new note(set_name.c_str(), CALI_ATTR_ASVALUE);
        feature_notes.insert({set_name, (void *) n});
    }

    return;
}

double
Apollo::getFeature(std::string req_name)
{
    double retval = 0.0;

    for(Apollo::Feature ft : features) {
        if (ft.name == req_name) {
            retval = ft.value;
            break;
        }
    };

    return retval;
}

void
Apollo::noteBegin(std::string &name, double with_value) {
    note *feat_annotation = (note *) getNote(name);
    if (feat_annotation != nullptr) {
        feat_annotation->begin(with_value);
    }
    return;
}

void
Apollo::noteEnd(std::string &name) {
    note *feat_annotation = (note *) getNote(name);
    if (feat_annotation != nullptr) {
        feat_annotation->end();
    }
    return;
}


void *
Apollo::getNote(std::string &name) {
    auto iter_feature = feature_notes.find(name);
    if (iter_feature != feature_notes.end()) {
        return (void *) iter_feature->second;
    } else {
        return nullptr;
    }
}




Apollo::Region *
Apollo::region(const char *regionName)
{
    auto search = regions.find(regionName);
    if (search != regions.end()) {
        return search->second;
    } else {
        return NULL;
    }

}

std::string
Apollo::uniqueRankIDText(void)
{
    std::stringstream ss_text;
    ss_text << "{";
    ss_text << "hostname: \"" << pub->node_id      << "\",";
    ss_text << "pid: \""      << pub->process_id   << "\",";
    ss_text << "mpi_rank: \"" << pub->comm_rank    << "\"";
    ss_text << "}";
    return ss_text.str();
}

int
Apollo::sosPack(const char *name, int val)
{
    return SOS_pack(pub, name, SOS_VAL_TYPE_INT, &val);
}

int
Apollo::sosPackRelated(long relation_id, const char *name, int val)
{
    return SOS_pack_related(pub, relation_id, name, SOS_VAL_TYPE_INT, &val);
}

void Apollo::sosPublish()
{
    if (isOnline()) {
        SOS_publish(pub);
    }
}


bool Apollo::isOnline()
{
    return ynConnectedToSOS;
}



