CONTIKI_PROJECT = sensor sensor_root
all: $(CONTIKI_PROJECT) 

CONTIKI = ../..

PROJECT_SOURCEFILES += message.c
PROJECT_SOURCEFILES += node.c
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

CONTIKI_WITH_RIME = 1

include $(CONTIKI)/Makefile.include
