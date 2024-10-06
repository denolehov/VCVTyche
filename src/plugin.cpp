#include "plugin.hpp"


Plugin* pluginInstance;


void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelOmen);
	p->addModel(modelTale);
	p->addModel(modelKron);
	p->addModel(modelFate);
	p->addModel(modelBlank);
	p->addModel(modelMoira);
}
