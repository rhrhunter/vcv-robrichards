#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
        pluginInstance = p;

        // Add modules here
        p->addModel(modelDarkworld);
        p->addModel(modelWarpedVinyl);
        p->addModel(modelMood);
        p->addModel(modelGenerationLoss);
        p->addModel(modelThermae);
        p->addModel(modelBlooper);
        p->addModel(modelPreampMKII);

        // Any other plugin initialization may go here.
        // As an alternative, consider lazy-loading assets and lookup tables when
        // your module is created to reduce startup times of Rack.
}
