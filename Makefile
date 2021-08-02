#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := crystal

.PHONY: incBUILDNR

default: incBUILDNR all

ifeq (,$(wildcard BUILDNR))
$(echo "1" > BUILDNR)
else
v=$(shell cat BUILDNR)
$(eval v=$(shell echo $$(($(v) + 1))))
buildnr=$(v)
endif

incBUILDNR:
	echo $(buildnr) > BUILDNR
	echo BUILDNR=$(buildnr)

PROJECT_VER="v1.0"
CXXFLAGS += -DBUILDNR=\"$(buildnr)\" -DPROJECT_VER=\"$(PROJECT_VER)\"

include $(IDF_PATH)/make/project.mk

deploy: $(APP_BIN)
	scp $(APP_BIN) vcspiel:/var/www/share
