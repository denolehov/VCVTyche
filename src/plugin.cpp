#include "plugin.hpp"


Plugin* pluginInstance;


void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelOracle);
	p->addModel(modelTale);
	p->addModel(modelKron);
	p->addModel(modelBlank);
}
