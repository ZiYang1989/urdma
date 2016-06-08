# config/subdir.mk

# This file is identical to mk/rte.extsubdir.mk in DPDK 2.2.0, except that it
# sets the current directory to the build directory, which allows kernel
# modules to build outside the build tree (module.mk will symlink the sources
# in the build directory).

#   BSD LICENSE
#
#   Copyright(c) 2014 6WIND S.A.
#   Copyright (c) 2016, IBM Corporation
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of 6WIND S.A. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

MAKEFLAGS += --no-print-directory

# output directory
O ?= .
BASE_OUTPUT ?= $(O)
CUR_SUBDIR ?= .

.PHONY: all
all: $(DIRS-y)

.PHONY: clean
clean: $(DIRS-y)

.PHONY: $(DIRS-y)
$(DIRS-y):
	mkdir -p $(BASE_OUTPUT)/$(CUR_SUBDIR)/$(@)/$(RTE_TARGET)
	@echo "== $@"
	$(Q)$(MAKE) -C $(BASE_OUTPUT)/$(CUR_SUBDIR)/$(@)/$(RTE_TARGET) \
		-f $(RTE_SRCDIR)/$(@)/Makefile \
		M=$(RTE_SRCDIR)/$(@)/Makefile \
		O=$(BASE_OUTPUT)/$(CUR_SUBDIR)/$(@)/$(RTE_TARGET) \
		BASE_OUTPUT=$(BASE_OUTPUT) \
		CUR_SUBDIR=$(CUR_SUBDIR)/$(@) \
		S=$(RTE_SRCDIR)/$(@) \
		$(filter-out $(DIRS-y),$(MAKECMDGOALS))
