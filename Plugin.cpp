#include "Plugin.h"
#include "lv2/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"

using namespace nlohmann ;
void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

void Plugin::free () {
    IN
    if (type == SharedLibrary::LADSPA)
        descriptor->cleanup (handle);
    else
        lv2Descriptor->cleanup (handle);
    OUT
}

Plugin::Plugin (const LADSPA_Descriptor * _descriptor, unsigned long _sampleRate, SharedLibrary::PluginType _type) {
    type = _type;
    if (_descriptor == NULL) {
        LOGF ("[%s:%s] null descriptor passed", __FILE__, __PRETTY_FUNCTION__ );
        return ;
    }

    descriptor = _descriptor ;
    if (_sampleRate > 0)
        sampleRate = _sampleRate ;
    else {
        LOGD ("[%s: %s] 0 sample rate passed", __FILE__, __PRETTY_FUNCTION__ );
        sampleRate = 48000 ;
    }

    if (type == SharedLibrary::LADSPA) {
        //~ LOGD("Creating plugin: %s", _descriptor->Name);
        lv2_name = std::string  (_descriptor->Name);

        handle = (LADSPA_Handle *) descriptor->instantiate(descriptor, sampleRate);
        ID = descriptor->UniqueID;
        //~ LOGD("[%s] loaded plugin %s [%d: %s] at %u", __PRETTY_FUNCTION__, descriptor->Name,
             //~ descriptor->UniqueID, descriptor->Label, sampleRate);
        //~ print();

        for (int i = 0; i < descriptor->PortCount; i++) {
            LADSPA_PortDescriptor port = descriptor->PortDescriptors[i];
            if (LADSPA_IS_PORT_AUDIO(port)) {
                if (LADSPA_IS_PORT_INPUT(port)) {
                    //~ LOGD("[%s %d]: found input port", descriptor->Name, i);
                    if (inputPort == -1)
                        inputPort = i;
                    else if (inputPort2 == -1)
                        inputPort2 = i;
                    else
                        LOGE("[%s %d]: %s is third input port", descriptor->Name, i,
                             descriptor->PortNames[i]);
                } else if (LADSPA_IS_PORT_OUTPUT(port)) {
                    //~ LOGD("[%s %d]: found output port", descriptor->Name, i);
                    if (outputPort == -1)
                        outputPort = i;
                    else if (outputPort2 == -1)
                        outputPort2 = i;
                    else
                        LOGE("[%s %d]: %s is third output port", descriptor->Name, i,
                             descriptor->PortNames[i]);

                }
            } else if (/*LADSPA_IS_PORT_OUTPUT(port)*/ false) {
                LOGE("[%s:%d] %s: ladspa port is output but not audio!", descriptor->Name, i,
                     descriptor->PortNames[i]);
                // this, erm, doesn't work
                /*
                if (outputPort == -1)
                    outputPort = port ;
                */
            } else if (LADSPA_IS_PORT_CONTROL(port) && LADSPA_IS_PORT_INPUT(port)) {
                //~ LOGD("[%s %d]: found control port", descriptor->Name, i);
                PluginControl *pluginControl = new PluginControl(descriptor, i);
                descriptor->connect_port(handle, i, pluginControl->def);
                pluginControls.push_back(pluginControl);
            } else if (LADSPA_IS_PORT_CONTROL(port) && LADSPA_IS_PORT_OUTPUT(port)) {
                //~ LOGD("[%s %d]: found possible monitor port", descriptor->Name, i);
                descriptor->connect_port(handle, i, &dummy_output_control_port);
            } else {
                // special case, aaaargh!
                if (descriptor->UniqueID == 2606) {
                    if (i == 2)
                        inputPort = i;
                    if (i == 3)
                        outputPort = i;
                    if (i == 0 || i == 1) {
                        PluginControl *pluginControl = new PluginControl(descriptor, i);
                        descriptor->connect_port(handle, i, pluginControl->def);
                        pluginControls.push_back(pluginControl);

                        if (i == 0) {
                            pluginControl->min = 0;
                            pluginControl->max = 25;
                        } else if (i == 1) {
                            pluginControl->min = -24;
                            pluginControl->max = 24;
                        }
                    }
                } else {
                    LOGE("[%s %d]: unknown port %s for %s (%d)", descriptor->Name, i,
                         descriptor->PortNames[i], descriptor->Label, descriptor->UniqueID);
                    descriptor->connect_port(handle, i, &dummy_output_control_port);
                }
            }
        }

        //> WARNING: Moving this here because ports have to be connected first
        //  before activating a plugin
        if (descriptor->activate) {
            descriptor->activate(handle);
        }
    } else /* if (type == SharedLibrary::LV2 ) */ {
        //~ LOGD("[LV2] waiting for shared library pointer ...") ;
        lv2Descriptor = (LV2_Descriptor *) descriptor ;

    }
}

void Plugin::print () {
    //~ LOGD("--------| Controls for %s: %d |--------------", descriptor->Name, descriptor ->PortCount) ;
    for (int i = 0 ; i < pluginControls.size() ; i ++) {
        pluginControls.at(i)->print();
    }
    LOGD ("input ports: %d, %d\n", inputPort, inputPort2) ;
    LOGD ("output ports: %d, %d\n", outputPort, outputPort2);
}

void Plugin::load () {
    IN
    lv2FeaturesInit();
    
    HERE
    //~ LOGD("Creating plugin: %s from %s @ %s\n", lv2Descriptor->URI, sharedLibrary->LIBRARY_PATH.c_str(), sharedLibrary->so_file.c_str());
    std::string lib_path = sharedLibrary->LIBRARY_PATH + "/" + sharedLibrary -> so_file + ".lv2/" ;
    LOGD("[LV2] library path: %s\n", lib_path.c_str());

    if (lv2Descriptor == NULL) {
        HERE LOGF("[LV2] lv2Descriptor is NULL, we will probably crash ...!\nplugin: %s\n", sharedLibrary->so_file.c_str());
    } 
    //~ else
        //~ LOGD ("[LV2] descriptor is ok, instantiating handle at %d ...\n", sampleRate);

//    handle = (LADSPA_Handle *) lv2Descriptor->instantiate(lv2Descriptor, sampleRate, lib_path.c_str(), sharedLibrary->featurePointers());
    handle = (LADSPA_Handle *) lv2Descriptor->instantiate(lv2Descriptor, sampleRate, lib_path.c_str(), features.data());
//    handle = (LADSPA_Handle *) lv2Descriptor->instantiate(lv2Descriptor, sampleRate, lib_path.c_str(), sharedLibrary->feature_list);
    if (handle == NULL)
        LOGF("[LV2] plugin handle is NULL, we will probably crash ...!\n") ;
    //~ else
        //~ LOGD("[LV2] Handle instantiated ok! Congratulations\n");

    std::string _uri_ = std::string (lv2Descriptor -> URI);
    replaceAll (_uri_, ":", "_");
    std::string json_ ;
    if (type == SharedLibrary::PluginType::LV2)
#ifdef __ANDROID__
        json_ = getLV2JSON(_uri_.c_str());
#else 
        json_ = getLV2JSON_PC(_uri_.c_str());
#endif
#ifndef __ANDROID__
    else if (type == SharedLibrary::PluginType::LILV)
        json_ = getLV2JSON_PC(_uri_.c_str());
#endif
    //~ LOGD ("parsing json: %s\n", json_.c_str ());
    json j = json::parse(json_);
    lv2_name = j ["-1"]["pluginName"];
    if (j["-1"].contains("prefix"))
        prefix = j ["-1"]["prefix"];

    //~ LOGD("[LV2 JSON] %s", std::string (j ["1"]["name"]).c_str());
    for (auto& el : j.items())
    {
        //~ LOGD("[LV2] %s", el.key().c_str());
        //~ LOGD("[LV2] %s -> %s", el.key().c_str(), el.value().dump().c_str());
        if (el.key () == "-1") {
            continue ;
        }

        json jsonPort = json::parse (el.value ().dump ());
        std::string portNameStr = std::string (jsonPort ["name"]);
        const char * portName = portNameStr.c_str ();
        // ayyo why ...?
        // this used to be the following
        // i can;t remember why tho
        const char * pluginName = sharedLibrary->so_file.c_str() ;
//        const char * pluginName = lv2_name.c_str();

        LADSPA_PortDescriptor port = jsonPort .find ("index").value();
        LOGD("[%s %s:%d]", pluginName, portName, port);
        if (jsonPort.find ("AudioPort") != jsonPort.end ()) {
            if (jsonPort.find ("InputPort")  != jsonPort.end ()) {
                //~ LOGD("[%s %d]: found input port", portName, port);
                if (inputPort == -1)
                    inputPort = port;
                else if (inputPort2 == -1)
                    inputPort2 = port;
                else
                    LOGE("[%s %d]: %s is third input port", pluginName, port, portName);
            } else if (jsonPort.find ("OutputPort")  != jsonPort.end ()) {
                //~ LOGD("[%s %d]: found output port", pluginName, port);
                if (outputPort == -1)
                    outputPort = port;
                else if (outputPort2 == -1)
                    outputPort2 = port;
                else
                    LOGE("[%s %d]: %s is third output port",
                         pluginName, port, portName);
            }
        } else if (jsonPort.find ("InputPort") != jsonPort.end() && jsonPort.find ("ControlPort") != jsonPort.end()) {
            //~ LOGD("[%s %d]: found control port", pluginName, port);
            int pluginIndex = addPluginControl(lv2Descriptor, jsonPort) - 1;
            lv2Descriptor->connect_port(handle, port, pluginControls.at (pluginIndex) ->def);
        } else if (jsonPort.find ("OutputPort") != jsonPort.end() && jsonPort.find("ControlPort") != jsonPort.end()) {
            LOGD("[%s %d]: found possible monitor port", lv2Descriptor->URI, port);
            lv2Descriptor->connect_port(handle, port, &dummy_output_control_port);
        } else if (jsonPort.find ("AtomPort") != jsonPort.end() && jsonPort.find ("InputPort") != jsonPort.end()) {
            LOGD ("configuring atom control port");
            if (filePort == nullptr) {
                int portSize = (int) jsonPort.find("minimumSize").value() + sizeof(LV2_Atom_Sequence) + sizeof (LV2_Atom) + 1 ;
                filePort = (LV2_Atom_Sequence *) malloc(portSize);
                filePort->atom.size = portSize - 1;
                LOGD ("file port allocated ok");
                /*
                 *  Implement map properly here
                 *  it's simply a function? that returns an int approximation
                 *  of a string
                 *  i think rest of it is done
                 */

                ampMap = ampMap_new ();
                ampMap->handle = symap;
//                ampMap->map = ampMap_map ;
                ampAtom = new AmpAtom (ampMap, portSize);
                filePortSize = portSize;
            }

            std::string uri_ = std::string (prefix).append(portName);
            int pluginIndex = addPluginControl(lv2Descriptor, jsonPort) - 1;

            pluginControls.at(pluginIndex)->urid = ampAtom->urid_map->map (ampAtom->urid_map->handle, uri_.c_str());
            LOGD ("[urid] %s -> %d", uri_.c_str(), pluginControls.at(pluginIndex)->urid);
            lv2Descriptor->connect_port(handle, port, filePort);

            LOGD("[%s %d/%d]: found possible atom port", lv2Descriptor->URI, port, pluginIndex);
        } else if (jsonPort.find ("AtomPort") != jsonPort.end() && jsonPort.find ("OutputPort") != jsonPort.end()) {
            if (notifyPort == nullptr) {
                notifyPort = (LV2_Atom_Sequence *) malloc((int) jsonPort.find("minimumSize").value() + sizeof (LV2_Atom_Sequence) + sizeof (LV2_Atom) + 1);
                notifyPort->atom.size = (int) jsonPort.find("minimumSize").value() + sizeof (LV2_Atom_Sequence) + sizeof (LV2_Atom)  ;
//                notifyPort->atom.type =

            }

            lv2Descriptor->connect_port(handle, port, notifyPort);
//            lv2Descriptor->connect_port(handle, port, filePort);

            LOGD("[%s %d]: found possible notify port", lv2Descriptor->URI, port);
        } else {
            LOGD("[LV2] Cannot understand port %d of %s: %s", port, pluginName, portName);
        }

//        std::cout << "key: " << el.key() << ", value:" << el.value() << '\n';
    }

    // this is here because activate may use "ports" which are allocated above
    if (lv2Descriptor->activate) {
        lv2Descriptor->activate(handle);
    }

    /*
    recursive_iterate(*this, j, [*this, &j](Plugin *plugin, json::const_iterator it){
        json jsonPort = (json) it.value() ;
        const char * portName = std::string (jsonPort ["name"]).c_str ();
        const char * pluginName = plugin->sharedLibrary->so_file.c_str() ;

        LADSPA_PortDescriptor port = jsonPort ["index"];
        if (jsonPort.find ("AudioPort") != jsonPort.end ()) {
            if (jsonPort.find ("InputPort")  != jsonPort.end ()) {
                LOGD("[%s %d]: found input port", portName, port);
                if (plugin->inputPort == -1)
                    plugin->inputPort = port;
                else if (plugin->inputPort2 == -1)
                    plugin->inputPort2 = port;
                else
                    LOGE("[%s %d]: %s is third input port", pluginName, port, portName);
            } else if (jsonPort.find ("OutputPort")  != jsonPort.end ()) {
                LOGD("[%s %d]: found output port", pluginName, port);
                if (plugin->outputPort == -1)
                    plugin->outputPort = port;
                else if (plugin ->outputPort2 == -1)
                    plugin->outputPort2 = port;
                else
                    LOGE("[%s %d]: %s is third output port",
                         pluginName, port, portName);
            }
        } else if (jsonPort.find ("InputPort") != jsonPort.end() && jsonPort.find ("ControlPort") != jsonPort.end()) {
            LOGD("[%s %d]: found control port", pluginName, port);
            int pluginIndex = plugin -> addPluginControl(plugin->lv2Descriptor, j);
            lv2Descriptor->connect_port(handle, port, plugin -> pluginControls.at (pluginIndex) ->def);
        } else if (jsonPort.find ("OutputPort") != jsonPort.end() && jsonPort.find("ControlPort") != jsonPort.end()) {
            LOGD("[%s %d]: found possible monitor port", lv2Descriptor->URI, port);
//            lv2Descriptor->connect_port(handle, port, &dummy_output_control_port);
        } else {
            LOGD("[LV2] Cannot understand port %d of %s: %s", port, pluginName, portName);
        }
    });
     */

    lv2ConnectWorkers();
    OUT
}

std::string Plugin::getLV2JSON (std::string pluginName) {
    IN
    //~ LOGD("[LV2] getting JSON for %s/%s", sharedLibrary->so_file.c_str(), pluginName.c_str());
    JNIEnv *env;
    sharedLibrary -> vm-> GetEnv((void**)&env, JNI_VERSION_1_6);
    if (env == NULL) {
        LOGF("cannot find env!");
    }

    jstring jstr1 = env->NewStringUTF(pluginName.c_str());
    jstring libname = env->NewStringUTF(sharedLibrary->so_file.c_str());

    if (sharedLibrary->mainActivityClassName == "")
        sharedLibrary->mainActivityClassName = std::string ("com/shajikhan/ladspa/amprack/MainActivity");
    jclass clazz = env->FindClass(sharedLibrary->mainActivityClassName.c_str());
    if (clazz == nullptr) {
        HERE LOGF("cannot find class!");
    }

    jmethodID mid = env->GetStaticMethodID(clazz, "getLV2Info",
                                           "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    if (mid == nullptr) {
        LOGF("cannot find method!");
    }

    jobject obj = env->CallStaticObjectMethod(clazz, mid, libname, jstr1);
    if (obj == nullptr) {
        LOGF("cannot find class!");
    }

    jstring retStr = (jstring)obj;
    const char *nativeString = env->GetStringUTFChars(retStr, 0);
    std::string str = std::string (nativeString);
    env->ReleaseStringUTFChars(retStr, nativeString);

    OUT
    return str;
}


int Plugin::addPluginControl (const LV2_Descriptor * _descriptor, nlohmann::json _j) {
    IN ;
    PluginControl * pluginControl = new PluginControl(_descriptor, _j);
    pluginControls.push_back(pluginControl);
    OUT ;
    return pluginControls.size();
}

void Plugin::setBuffer (float * buffer, int read_bytes) {
    IN
    // dangerous non standard stuff
    // dont try this at home
    if (type == SharedLibrary::LV2) {
        //~ LOGD("setting buffer for LV2 plugin") ;
        lv2Descriptor->connect_port(handle, 99, &read_bytes);
        lv2Descriptor->connect_port(handle, 100, buffer);
    } else {
        //~ LOGD("setting buffer for LADSPA plugin");
        descriptor->connect_port(handle, 99, reinterpret_cast<LADSPA_Data *>(&read_bytes));
        descriptor->connect_port(handle, 100, buffer);
    }
    OUT
}

void Plugin::setFileName (std::string filename) {
    IN
    float s = filename.size() ;
    lv2Descriptor->connect_port(handle, 99, (void *) &s);
    lv2Descriptor->connect_port(handle, 100, (void *) filename.c_str());
    loadedFileName = std::string (filename);
    loadedFileType = 1 ;
    OUT
}

void Plugin::lv2FeaturesURID () {
//    lv2UridMap.handle = &urid ;
    symap = symap_new();
    lv2UridMap.handle = symap;
//    lv2UridMap.map = reinterpret_cast<LV2_URID (*)(LV2_URID_Map_Handle, const char *)>(lv2_urid_map);
    lv2UridMap.map = reinterpret_cast<LV2_URID (*)(LV2_URID_Map_Handle, const char *)>(symap_map);
//    lv2UridMap.unmap = reinterpret_cast<LV2_URID (*)(LV2_URID_Map_Handle, const char *)>(lv2_urid_unmap);

    featureURID.URI = strdup (LV2_URID__map);
    featureURID.data = &lv2UridMap ;

    logLog.handle = NULL ;
    logLog.printf = logger_printf ;
    logLog.vprintf = logger_vprintf ;

    featureLog.URI = strdup (LV2_LOG__log) ;
    featureLog.data = &logLog ;

    lv2WorkerSchedule.schedule_work = lv2ScheduleWork ;
    lv2WorkerSchedule.handle = this;
    featureSchedule.URI = strdup (LV2_WORKER__schedule);
    featureSchedule.data = &lv2WorkerSchedule ;

    LV2_Options_Interface optionsInterface ;
    optionsInterface.get = lv2_options_get;
    optionsInterface.set = lv2_options_set;
    featureState.URI = strdup (LV2_OPTIONS__options);
    featureState.data = &optionsInterface;

    features.push_back(&featureURID);
    features.push_back(&featureLog);
    features.push_back(&featureSchedule);
    features.push_back(&featureState);
    features.push_back(nullptr);
}

void Plugin::lv2FeaturesInit () {
    IN
    lv2FeaturesURID();

    OUT
}

void Plugin::lv2ConnectWorkers () {
    if (lv2Descriptor->extension_data == nullptr)
        return;
    lv2WorkerInterface = (LV2_Worker_Interface *) lv2Descriptor->extension_data (LV2_WORKER__interface);
    lv2StateInterface = (LV2_State_Interface *) lv2Descriptor->extension_data (LV2_STATE__interface);
}

LV2_Worker_Status lv2ScheduleWork (LV2_Worker_Schedule_Handle handle, uint32_t size, const void * data) {
    IN
    Plugin * plugin = reinterpret_cast<Plugin *>(handle);
    LV2_Worker_Status status = plugin->lv2WorkerInterface->work (plugin->handle, plugin->lv2WorkerInterface->work_response, plugin->handle, size, data);

//    plugin->check_notify();
    OUT
    return status ;
}

uint32_t lv2_options_set (LV2_Handle instance, const LV2_Options_Option* options) {
    return 0u;
}

uint32_t lv2_options_get (LV2_Handle instance, LV2_Options_Option* options) {
    return 0u;
}

void Plugin::setAtomPortValue (int control, std::string text) {
    IN
    if (filePort == nullptr) {
        LOGD("no atom control port for %s", lv2_name.c_str());
        OUT
        return ;
    }

    /*  some mechanism here to figure out which button was clicked
     *  on the plugin. maybe separate with | ?
     */
//    ampAtom->sendFilenameToPlugin(filePort, text.c_str());
//    ampAtom->send_filename_to_plugin(ampMap, text.c_str(), reinterpret_cast<uint8_t *>(filePort), 8192 + sizeof (LV2_Atom));
//    ampAtom->son_of_a(filePort, text.c_str());
    LOGD ("writing control for %d / %s [%d]", control, pluginControls.at(control)->name, pluginControls.at(control)->urid);
    ampAtom->write_control(filePort, filePortSize, pluginControls.at(control)->urid, text.c_str());
//    ampAtom->write_control(notifyPort, filePortSize, text.c_str());
//    ampAtom->setControl(filePort, const_cast<char *>(text.c_str()));
    OUT
}

void Plugin::setFilePortValue (std::string filename) {
    IN
    int size = filename.size();
    char * str = (char * ) malloc (filename.size() + 1);
    strcpy(str, filename.c_str());
    lv2Descriptor->connect_port(handle, 9, &size);
    lv2Descriptor->connect_port(handle, 4, str);
    OUT
}

void Plugin::setFilePortValue1 (std::string filename) {
    IN
    if (filePortIndex == -1) {
        LOGE("set file port requested but no file port available") ;
        return;
    }

    //~ LOGD("[atom sequence] %s", filename.c_str());
#ifdef USE_THIS
    LV2_Atom_Forge       forge ;

    LV2_Atom_Forge_Frame frame;
    LV2_URID_Map map ;
    map.handle = &urid;
    map.map = reinterpret_cast<LV2_URID (*)(LV2_URID_Map_Handle, const char *)>(lv2_urid_map);
    lv2_atom_forge_init(&forge, &map);
    LV2_Atom* set = (LV2_Atom*)lv2_atom_forge_blank(
            &forge, &frame, 1, 9);

    lv2_atom_forge_property_head(&forge, 10, 0);
    lv2_atom_forge_urid(&forge, 6);
    lv2_atom_forge_property_head(&forge, 11, 0);
    lv2_atom_forge_path(&forge, filename.c_str(), filename.size());

    lv2_atom_forge_pop(&forge, &frame);
    lv2_atom_forge_sequence_head(&forge, &frame, 0);

    uint8_t buf [1024];
    lv2_atom_forge_set_buffer(&forge,
                              (uint8_t *) filePort,
                              1024);



    LV2_Atom_Object lv2AtomObject;
    lv2AtomObject.body.otype = 9 ;
#else
    LV2_Atom_Forge       forge ;
    LV2_Atom_Forge_Frame frame;
    uint8_t              buf[1024];
    lv2_atom_forge_set_buffer(&forge, buf, sizeof(buf));

    lv2_atom_forge_object(&forge, &frame, 0, 9);
    lv2_atom_forge_key(&forge, 10);
    lv2_atom_forge_urid(&forge, 6);
    lv2_atom_forge_key(&forge, 11);
    lv2_atom_forge_atom(&forge, filename.size(), 12);
    lv2_atom_forge_write(&forge, filename.c_str(), filename.size());

    const LV2_Atom* atom = lv2_atom_forge_deref(&forge, frame.ref);
    typedef struct {
        uint32_t index;
        uint32_t protocol;
        uint32_t size;
        // Followed immediately by size bytes of data
    } ControlChange;
    typedef struct {
        ControlChange change;
        LV2_Atom      atom;
    } Header;
    const Header header = {
            {0, 5, (uint32_t) (sizeof(LV2_Atom) + filename.size())},
            {uint32_t (filename.size()), 12}};

//    lv2Descriptor->connect_port(handle, filePortIndex, filePort);
    memcpy(filePort, &header, sizeof  (header));
    memcpy(filePort + sizeof (header), &atom, sizeof  (atom));
    __atomic_store_n(&filePort, filePort, __ATOMIC_RELEASE) ;

    lv2Descriptor->connect_port(handle, filePortIndex, filePort);

#endif
    LV2_ATOM_SEQUENCE_FOREACH(filePort, ev) {
        const LV2_Atom_Object *obj = (LV2_Atom_Object *) &ev->body;
        //~ LOGD ("[command] %d", obj->body.otype);
        const LV2_Atom* property = NULL;
        lv2_atom_object_get(obj, 10, &property, 0);
        //~ LOGD ("%s", property);

    }
    OUT
}

bool Plugin::check_notify () {
    //~ IN
    if (notifyPort == nullptr) {
        //~ LOGD ("notify port is null, so .. well.. this is awkward");
        OUT
        return true;
    }

    if (ampAtom != nullptr && filePort != nullptr && ampAtom->has_file_path(filePort)) {
        //~ LOGD ("[atom port] reset file port");
        ampAtom->resetAtom(filePort, filePortSize);
        //~ LOGD ("[atom port] reset notify port");
        ampAtom->resetAtom(notifyPort, filePortSize);
        //~ OUT
        return true ;
    }

    //~ OUT
    return false;
}

#ifndef __ANDROID__
std::string Plugin::getLV2JSON_PC (std::string pluginName) {
    IN
    //~ HERE LOGD ("[%s] plugin: %s\n", sharedLibrary->so_file.c_str (), pluginName.c_str ());
    // todo:
    // file name here, load and return this json file
    // rename lv2 directories !
    std::string stub = std::string (lv2Descriptor->URI) ;
    if (stub.find ("#" ) != -1)
        stub = stub.substr (stub.find ("#") + 1, stub.size ()) ;
    else 
        stub = stub.substr (stub.find_last_of ("/") + 1, stub.size ()) ;
        
    std::string lib = std::string (sharedLibrary->so_file);
    lib = lib.substr (lib.find_last_of ("/") + 1, lib.size ());
    std::string path = sharedLibrary->lv2_config_path ;
    path.append ("/").append (lib).append ("/").append (stub).append (".json");
    std::replace(path.begin(), path.end(), ':', '_');

    LOGD ("[LV2 %s] config for %s: %s\n", stub.c_str (), pluginName.c_str (), path.c_str ());
    std::ifstream fJson(path.c_str ());
    std::stringstream buffer;
    buffer << fJson.rdbuf();
    OUT
    return buffer.str () ;
    
    /*
    auto json = nlohmann::json::parse(buffer.str());
    std::string name (lilv_node_as_string (lilv_plugin_get_name (sharedLibrary->plugin)));

    for (auto plugin : json) {
        std::string s = plugin ["name"].dump();
        std::string ss = s.substr (1, s.size() - 2) ;
        //~ printf ("comparing plugin: %s | %s \n", ss.c_str (), name.c_str ());
        if (ss == name) {
            printf ("found config for plugin: %s\n", pluginName.c_str ());
            
            sharedLibrary->LIBRARY_PATH = plugin["library"].dump ();
            sharedLibrary->LIBRARY_PATH = sharedLibrary->LIBRARY_PATH.substr (1, sharedLibrary->LIBRARY_PATH.size () - 2);
            lv2_name = ss ;
            break ;
        }
    }
    
    std::string stub = std::string (lv2Descriptor->URI);
    //~ printf ("%s > %d\n", lv2Descriptor->URI, stub.find ("#"));
    if (stub.find ("#") == -1) {
        stub = stub.substr (stub.find_last_of ("/") + 1, stub.size () - 1) ;
        printf ("stub: %s\n", stub.c_str ());
    } else {
        stub = stub.substr (stub.find ("#"), stub.size () - 1);
    }
    
    std::string path = std::string ("assets/lv2/").append (sharedLibrary->LIBRARY_PATH).append ("/").append (stub).append (".json").c_str () ;
    printf ("path: %s\n", path.c_str());
    std::ifstream fson(path.c_str ());
    std::stringstream buffer_;
    buffer_ << fson.rdbuf();
    //~ printf ("[plugin] json: %s\n", buffer.str ().c_str ());
    //~ json = nlohmann::json::parse(buffer.str());
    
    OUT
    return buffer_.str ();
    */
}
#endif

#define BUFFER_SIZE 512


// Options and Atom support
typedef struct {
    LV2_URID atom_Int;
    LV2_URID atom_Float;
    LV2_URID atom_Path;
    LV2_URID atom_String;
    LV2_URID atom_Sequence;
    LV2_URID atom_eventTransfer;
    LV2_URID patch_Set;
    LV2_URID patch_property;
    LV2_URID patch_value;
    LV2_URID bufsz_minBlockLength;
    LV2_URID bufsz_maxBlockLength;
    LV2_URID bufsz_nominalBlockLength;
    LV2_URID bufsz_sequenceSize;
    LV2_URID param_sampleRate;
} OptionsURIDs;

static char* uris[1000];
static size_t n_uris = 0;
static float sample_rate = 48000;

static LV2_URID simple_map(LV2_URID_Map_Handle handle, const char* uri) {
    for (size_t i = 0; i < n_uris; i++) {
        if (strcmp(uris[i], uri) == 0) {
            return i + 1;
        }
    }
    if (n_uris < 1000) {
        uris[n_uris] = strdup(uri);
        return ++n_uris;
    }
    return 0;
}

// Global options and atom state
static OptionsURIDs options_urids;
static int32_t sequence_size = 8192;
static int32_t block_length = BUFFER_SIZE;


// Atom forge for creating atom messages
static LV2_Atom_Forge forge;
static uint8_t forge_buffer[8192];
static LV2_Atom_Sequence* input_sequence = NULL;
static LV2_Atom_Sequence* output_sequence = NULL;

// Function prototypes
static int pass_filename_to_plugin(const char* filename);

/*
 * Usage example:
 *   pass_filename_to_plugin("/path/to/sample.wav");
 *   pass_filename_to_plugin("/home/user/audio/kick.flac");
 *   pass_filename_to_plugin("./relative/path/snare.aiff");
 */

// Options get callback
static uint32_t options_get(LV2_Handle instance,
                           LV2_Options_Option* options) {
    (void)instance;  // Unused parameter
    for (LV2_Options_Option* opt = options; opt->key || opt->value; ++opt) {
        if (opt->key == options_urids.bufsz_minBlockLength ||
            opt->key == options_urids.bufsz_maxBlockLength ||
            opt->key == options_urids.bufsz_nominalBlockLength) {
            if (opt->size >= sizeof(int32_t)) {
                *(int32_t*)opt->value = block_length;
                opt->type = options_urids.atom_Int;
                opt->size = sizeof(int32_t);
            }
        } else if (opt->key == options_urids.bufsz_sequenceSize) {
            if (opt->size >= sizeof(int32_t)) {
                *(int32_t*)opt->value = sequence_size;
                opt->type = options_urids.atom_Int;
                opt->size = sizeof(int32_t);
            }
        } else if (opt->key == options_urids.param_sampleRate) {
            if (opt->size >= sizeof(float)) {
                *(float*)opt->value = sample_rate;
                opt->type = options_urids.atom_Float;
                opt->size = sizeof(float);
            }
        } else {
            opt->type = 0;  // Unsupported option
            opt->size = 0;
        }
    }
    return LV2_OPTIONS_SUCCESS;
}

// Options set callback  
static uint32_t options_set(LV2_Handle instance,
                           const LV2_Options_Option* options) {
    (void)instance;  // Unused parameter
    for (const LV2_Options_Option* opt = options; opt->key || opt->value; ++opt) {
        if (opt->key == options_urids.bufsz_minBlockLength ||
            opt->key == options_urids.bufsz_maxBlockLength ||
            opt->key == options_urids.bufsz_nominalBlockLength) {
            if (opt->type == options_urids.atom_Int && opt->size == sizeof(int32_t)) {
                block_length = *(const int32_t*)opt->value;
                printf("[Options] Set block length to %d\n", block_length);
            }
        } else if (opt->key == options_urids.bufsz_sequenceSize) {
            if (opt->type == options_urids.atom_Int && opt->size == sizeof(int32_t)) {
                sequence_size = *(const int32_t*)opt->value;
                printf("[Options] Set sequence size to %d\n", sequence_size);
            }
        } else if (opt->key == options_urids.param_sampleRate) {
            if (opt->type == options_urids.atom_Float && opt->size == sizeof(float)) {
                sample_rate = *(const float*)opt->value;
                printf("[Options] Set sample rate to %.0f\n", sample_rate);
            }
        }
    }
    return LV2_OPTIONS_SUCCESS;
}

// Create an atom:Path message
static LV2_Atom* create_path_atom(const char* path) {
    lv2_atom_forge_set_buffer(&forge, forge_buffer, sizeof(forge_buffer));
    
    LV2_Atom_Forge_Frame frame;
    LV2_Atom* atom = (LV2_Atom*)lv2_atom_forge_object(
        &forge, &frame, 0, options_urids.patch_Set);
    
    lv2_atom_forge_key(&forge, options_urids.patch_property);
    lv2_atom_forge_urid(&forge, options_urids.atom_Path);
    lv2_atom_forge_key(&forge, options_urids.patch_value);
    lv2_atom_forge_path(&forge, path, strlen(path));
    
    lv2_atom_forge_pop(&forge, &frame);
    
    return atom;
}

// Initialize atom sequence for plugin communication
static void init_atom_sequences() {
    const size_t seq_size = sizeof(LV2_Atom_Sequence) + 8192;
    
    input_sequence = (LV2_Atom_Sequence*)calloc(1, seq_size);
    output_sequence = (LV2_Atom_Sequence*)calloc(1, seq_size);
    
    input_sequence->atom.type = options_urids.atom_Sequence;
    input_sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
    input_sequence->body.unit = options_urids.atom_eventTransfer;
    input_sequence->body.pad = 0;
    
    output_sequence->atom.type = options_urids.atom_Sequence;
    output_sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
    output_sequence->body.unit = options_urids.atom_eventTransfer;
    output_sequence->body.pad = 0;
    
    printf("Initialized atom sequences for file path communication\n");
}

// Send a file path to plugin via atom:Path
static void send_file_path(const char* file_path) {
    if (!input_sequence) return;
    
    // Reset sequence
    input_sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
    
    // Create path atom
    LV2_Atom* path_atom = create_path_atom(file_path);
    if (path_atom) {
        // Add to sequence at frame 0
        LV2_Atom_Event event;
        event.time.frames = 0;
        event.body = *path_atom;
        lv2_atom_sequence_append_event(
            input_sequence, 8192,
            &event);
        
        printf("Sent file path via atom:Path: %s\n", file_path);
    }
}

// Pass a filename to the plugin with validation and error handling
static int pass_filename_to_plugin(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Error: Invalid filename provided\n");
        return -1;
    }
    
    if (!input_sequence) {
        printf("Error: Atom sequences not initialized\n");
        return -1;
    }
    
    // Check if file exists (optional validation)
    FILE* test_file = fopen(filename, "r");
    if (test_file) {
        fclose(test_file);
        printf("File exists, passing to plugin: %s\n", filename);
    } else {
        printf("Warning: File may not exist, but passing to plugin anyway: %s\n", filename);
    }
    
    // Reset sequence for new file
    input_sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
    
    // Create and send path atom
    lv2_atom_forge_set_buffer(&forge, forge_buffer, sizeof(forge_buffer));
    
    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_sequence_head(&forge, &frame, options_urids.atom_eventTransfer);
    
    // Add path event at frame 0
    lv2_atom_forge_frame_time(&forge, 0);
    lv2_atom_forge_object(&forge, &frame, 0, options_urids.patch_Set);
    lv2_atom_forge_key(&forge, options_urids.patch_property);
    lv2_atom_forge_urid(&forge, options_urids.atom_Path);
    lv2_atom_forge_key(&forge, options_urids.patch_value);
    lv2_atom_forge_path(&forge, filename, strlen(filename));
    
    lv2_atom_forge_pop(&forge, &frame);
    
    // Copy forged sequence to input sequence
    LV2_Atom_Sequence* seq = (LV2_Atom_Sequence*)forge.buf;
    memcpy(input_sequence, seq, sizeof(LV2_Atom_Sequence) + seq->atom.size);
    
    printf("Successfully passed filename to plugin: %s\n", filename);
    printf("  Atom sequence size: %u bytes\n", input_sequence->atom.size);
    printf("  Event time: 0 frames\n");
    
    return 0;
}

// Process atom sequences for file paths
static void process_atom_sequences() {
    if (!output_sequence) return;
    
    LV2_ATOM_SEQUENCE_FOREACH(output_sequence, ev) {
        const LV2_Atom* atom = &ev->body;
        
        if (atom->type == options_urids.atom_Path) {
            const char* path = (const char*)(atom + 1);
            printf("Received atom:Path from plugin: %s\n", path);
        } else if (atom->type == options_urids.atom_String) {
            const char* str = (const char*)(atom + 1);
            printf("Received atom:String from plugin: %s\n", str);
        }
    }
    
    // Reset output sequence for next cycle
    output_sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
}

Plugin::Plugin (char * _uri, unsigned long _sampleRate, LilvWorld * world, const LilvPlugins * _plugins) {
    IN
    uri = lilv_new_uri(world, _uri);
    sample_rate = (float) _sampleRate ;

    // Setup basic features
    LV2_URID_Map map = { NULL, simple_map };
    LV2_Feature map_feature = { LV2_URID__map, &map };
    
    // Initialize options and atom URIDs
    options_urids.atom_Int = simple_map(NULL, LV2_ATOM__Int);
    options_urids.atom_Float = simple_map(NULL, LV2_ATOM__Float);
    options_urids.atom_Path = simple_map(NULL, LV2_ATOM__Path);
    options_urids.atom_String = simple_map(NULL, LV2_ATOM__String);
    options_urids.atom_Sequence = simple_map(NULL, LV2_ATOM__Sequence);
    options_urids.atom_eventTransfer = simple_map(NULL, LV2_ATOM__eventTransfer);
    options_urids.patch_Set = simple_map(NULL, LV2_PATCH__Set);
    options_urids.patch_property = simple_map(NULL, LV2_PATCH__property);
    options_urids.patch_value = simple_map(NULL, LV2_PATCH__value);
    options_urids.bufsz_minBlockLength = simple_map(NULL, LV2_BUF_SIZE__minBlockLength);
    options_urids.bufsz_maxBlockLength = simple_map(NULL, LV2_BUF_SIZE__maxBlockLength);
    options_urids.bufsz_nominalBlockLength = simple_map(NULL, LV2_BUF_SIZE__nominalBlockLength);
    options_urids.bufsz_sequenceSize = simple_map(NULL, LV2_BUF_SIZE__sequenceSize);
    options_urids.param_sampleRate = simple_map(NULL, "http://lv2plug.in/ns/lv2core#sampleRate");
    
    // Setup options feature
    LV2_Options_Interface options_iface = { options_get, options_set };
    LV2_Feature options_interface_feature = { LV2_OPTIONS__interface, &options_iface };
    
    // Create options array with current values
    static int32_t block_len_val = BUFFER_SIZE;
    static float sample_rate_val = sampleRate;
    static int32_t seq_size_val = 8192;
    
    LV2_Options_Option options[] = {
        { LV2_OPTIONS_INSTANCE, 0, options_urids.bufsz_minBlockLength,
          sizeof(int32_t), options_urids.atom_Int, &block_len_val },
        { LV2_OPTIONS_INSTANCE, 0, options_urids.bufsz_maxBlockLength,
          sizeof(int32_t), options_urids.atom_Int, &block_len_val },
        { LV2_OPTIONS_INSTANCE, 0, options_urids.bufsz_nominalBlockLength,
          sizeof(int32_t), options_urids.atom_Int, &block_len_val },
        { LV2_OPTIONS_INSTANCE, 0, options_urids.bufsz_sequenceSize,
          sizeof(int32_t), options_urids.atom_Int, &seq_size_val },
        { LV2_OPTIONS_INSTANCE, 0, options_urids.param_sampleRate,
          sizeof(float), options_urids.atom_Float, &sample_rate_val },
        { LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, NULL }  // Terminator
    };
    
    LV2_Feature options_array_feature = { LV2_OPTIONS__options, options };
    
        const LV2_Feature* features[] = { 
        &map_feature, 
        &options_interface_feature,
        &options_array_feature,
        NULL 
    };
    
    // Initialize atom forge and sequences
    lv2_atom_forge_init(&forge, &map);
    init_atom_sequences();
    
    if (uri == nullptr) {
        LOGF ("[%s:%s] could not create lilv node from uri: %s", __FILE__, __PRETTY_FUNCTION__, _uri);
        return ;
    }

    lilv_plugin = lilv_plugins_get_by_uri(_plugins, uri);
    instance = lilv_plugin_instantiate(lilv_plugin, sampleRate, features);
    if (instance == nullptr) {
        LOGF ("[%s:%s] could not instantiate lilv plugin from uri: %s", __FILE__, __PRETTY_FUNCTION__, _uri);
        uri = nullptr;
        return ;
    } else 
        LOGD ("[%s:%s] instantiated lilv plugin from uri: %s at %d", __FILE__, __PRETTY_FUNCTION__, _uri, _sampleRate);

    lv2Descriptor = instance ->lv2_descriptor ;

    type = SharedLibrary::PluginType::LILV;
    sampleRate = _sampleRate ;

    const uint32_t n_ports = lilv_plugin_get_num_ports(lilv_plugin);
    float* min_values = (float*)calloc(n_ports, sizeof(float));
    float* max_values = (float*)calloc(n_ports, sizeof(float));
    float* def_values = (float*)calloc(n_ports, sizeof(float));

    float * dummy_output_control_port = (float *) malloc (sizeof (float));

    // Get the port ranges using the convenience function
    lilv_plugin_get_port_ranges_float(lilv_plugin, min_values, max_values, def_values);
    LilvNode* lv2_InputPort   = lilv_new_uri(world, LV2_CORE__InputPort);
    LilvNode* lv2_OutputPort  = lilv_new_uri(world, LV2_CORE__OutputPort);
    LilvNode* lv2_AudioPort   = lilv_new_uri(world, LV2_CORE__AudioPort);
    LilvNode* lv2_ControlPort = lilv_new_uri(world, LV2_CORE__ControlPort);
    LilvNode* lv2_AtomPort    = lilv_new_uri(world, LV2_ATOM__AtomPort);

    for (uint32_t i = 0; i < n_ports; ++i) {
        const LilvPort* port = lilv_plugin_get_port_by_index(lilv_plugin, i);
        if (lilv_port_is_a(lilv_plugin, port, lv2_AudioPort)) {
            if (lilv_port_is_a(lilv_plugin, port, lv2_OutputPort)) {
                //~ LOGD("[%s %d]: found output port", lilv_node_as_string(lilv_plugin_get_name(lilv_plugin)), i);
                if (outputPort == -1)
                    outputPort = i;
                else if (outputPort2 == -1)
                    outputPort2 = i;
                else
                    LOGE("[%s %d]: is third output port", lilv_node_as_string(lilv_plugin_get_name(lilv_plugin)), i);
            } else if (lilv_port_is_a(lilv_plugin, port, lv2_InputPort)) {
                //~ LOGD("[%s %d]: found input port", lilv_node_as_string(lilv_plugin_get_name(lilv_plugin)), i);
                if (inputPort == -1)
                    inputPort = i;
                else if (inputPort2 == -1)
                    inputPort2 = i;
                else
                    LOGE("[%s %d]: is third input port", lilv_node_as_string(lilv_plugin_get_name(lilv_plugin)), i);
            }

            // dummy connect audio ports
            lilv_instance_connect_port(instance, i, dummy_output_control_port);
            continue;            
        }

        if (lilv_port_is_a(lilv_plugin, port, lv2_ControlPort)) {
            if (lilv_port_is_a(lilv_plugin, port, lv2_InputPort) == false) {
                lilv_instance_connect_port(instance, i, dummy_output_control_port);
                continue;
            } else {
                PluginControl* pluginControl = new PluginControl(lilv_plugin, i);

                pluginControl->min = min_values[i];
                pluginControl->max = max_values[i];
                pluginControl->default_value = def_values[i];
                pluginControl->def = (LADSPA_Data *) malloc (sizeof(LADSPA_Data));
                lilv_instance_connect_port(instance, i, pluginControl->def);
                *pluginControl->def = def_values[i];

                pluginControl->lv2_name = std::string(lilv_node_as_string(lilv_port_get_name(lilv_plugin, port)));

                pluginControl->name = lilv_node_as_string(lilv_port_get_name(lilv_plugin, port));
                pluginControls.push_back(pluginControl);
                continue;
            }
        }

        if (lilv_port_is_a(lilv_plugin, port, lv2_AtomPort)) {
            PluginControl* pluginControl = new PluginControl(lilv_plugin, i);
            pluginControl->lv2_name = std::string(lilv_node_as_string(lilv_port_get_name(lilv_plugin, port)));
            pluginControl->name = lilv_node_as_string(lilv_port_get_name(lilv_plugin, port));
            pluginControl->lilv_port_index = i ;
            pluginControls.push_back(pluginControl);
            pluginControl->file_port_size = 8192;
            pluginControl->filePort = (LV2_Atom_Sequence *) malloc (pluginControl->file_port_size);
            memset(pluginControl->filePort, 0, pluginControl->file_port_size);
            lilv_instance_connect_port(instance, i, pluginControl->filePort);

            if (lilv_port_is_a(lilv_plugin, port, lv2_InputPort)) {
                LOGD("[%s %d]: found atom input port", lilv_node_as_string(lilv_plugin_get_name(lilv_plugin)), i);
                pluginControl->type = PluginControl::Type::LV2_ATOM_INPUT_PORT;
            } else {
                LOGD("[%s %d]: found atom output port", lilv_node_as_string(lilv_plugin_get_name(lilv_plugin)), i);
                pluginControl->type = PluginControl::Type::LV2_ATOM_OUTPUT_PORT;
            }

            continue;
        }

    }

    lilv_instance_activate(instance);
    process_atom_sequences();
    print();
    OUT
}