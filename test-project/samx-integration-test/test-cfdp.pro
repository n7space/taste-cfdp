TEMPLATE = lib
CONFIG -= qt
CONFIG += generateC

DISTFILES +=  $(HOME)/tool-inst/share/taste-types/taste-types.asn \
    $(HOME)/tool-inst/share/taste-types/taste-types.asn \
    $(HOME)/tool-inst/share/taste-types/taste-types.asn \
    LittleFS.acn \
    LittleFS.asn \
    memoryaccess.acn \
    memoryaccess.asn \
    samv71.dv.xml \
    taste-cfdp.acn \
    taste-cfdp.asn
DISTFILES += test-cfdp.msc
DISTFILES += interfaceview.xml
DISTFILES += work/binaries/*.msc
DISTFILES += work/binaries/coverage/index.html
DISTFILES += work/binaries/filters
DISTFILES += work/system.asn

DISTFILES += test-cfdp.asn
DISTFILES += test-cfdp.acn
include(work/taste.pro)
message($$DISTFILES)

