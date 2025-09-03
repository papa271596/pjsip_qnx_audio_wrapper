# qnx-config.mak

# Toolchain
CC      = qcc
CXX     = q++
LD      = qcc
AR      = qcc -ar

# Target architecture
CFLAGS  += -Vgcc_ntox86 -D__QNX__ -DPJ_QNX=1 -Wall -DPJ_HAS_CONFIG_SITE=1
CFLAGS  += -DPJMEDIA_AUDIO_DEV_HAS_OSS=0 -DPJ_HAS_CONFIG_SITE=1
CFLAGS  += -DPJMEDIA_AUDIO_DEV_HAS_ALSA=0 -DPJ_HAS_CONFIG_SITE=1
CFLAGS  += -I$(PJDIR)/pjlib/include -I$(PJDIR)/pjlib-util/include -I$(PJDIR)/pjmedia/include -I$(PJDIR)/pjsip/include -DPJ_HAS_CONFIG_SITE=1


# Include paths (adjust if needed)
CFLAGS  += -I$(PJDIR)/pjlib/include -I$(PJDIR)/pjlib-util/include -I$(PJDIR)/pjmedia/include -I$(PJDIR)/pjsip/include
