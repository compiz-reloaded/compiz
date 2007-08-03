SUBDIRS = include src libdecoration plugins images gtk kde po metadata

EXTRA_DIST =		    \
	COPYING		    \
	COPYING.GPL	    \
	COPYING.LGPL	    \
	COPYING.MIT	    \
	compiz.pc.in	    \
	compiz-cube.pc.in   \
	compiz-scale.pc.in  \
	compiz-gconf.pc.in  \
	intltool-extract.in \
	intltool-merge.in   \
	intltool-update.in  \
	intltool-extract    \
	intltool-merge	    \
	intltool-update

DISTCLEANFILES =	 \
	intltool-extract \
	intltool-merge   \
	intltool-update	 \
	po/stamp-it	 \
	po/.intltool-merge-cache

if USE_GCONF
gconfdata = compiz-gconf.pc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = compiz.pc compiz-cube.pc compiz-scale.pc $(gconfdata)

# Some custom targets to make it easier to release things.
# Use either:
#		make release-check
# or		make release-publish

RELEASE_UPLOAD_HOST =   xorg.freedesktop.org
RELEASE_UPLOAD_BASE =	/srv/xorg.freedesktop.org/archive/individual/app
RELEASE_UPLOAD_DIR =	$(RELEASE_UPLOAD_BASE)
RELEASE_URL_BASE = 	http://xorg.freedesktop.org/archive/individual/app
RELEASE_ANNOUNCE_LIST = compiz@list.freedesktop.org

tar_file  = $(PACKAGE)-$(VERSION).tar.gz
sha1_file = $(tar_file).sha1
gpg_file  = $(sha1_file).asc

$(sha1_file): $(tar_file)
	sha1sum $^ > $@

$(gpg_file): $(sha1_file)
	@echo "Please enter your GPG password to sign the checksum."
	gpg --armor --sign $^

release-verify-even-micro:
	@echo -n "Checking that $(VERSION) has an even micro component..."
	@test "$(COMPIZ_VERSION_MICRO)" = "`echo $(COMPIZ_VERSION_MICRO)/2*2 | bc`" \
		|| (echo "Ouch." && echo "The version micro component '$(COMPIZ_VERSION_MICRO)' is not an even number." \
		&& echo "The version in configure.in must be incremented before a new release." \
		&& false)
	@echo "Good."

release-verify-newer:
	@echo -n "Checking that no $(VERSION) release already exists..."
	@ssh $(RELEASE_UPLOAD_HOST) test ! -e $(RELEASE_UPLOAD_DIR)/$(tar_file) \
		|| (echo "Ouch." && echo "Found: $(RELEASE_UPLOAD_HOST):$(RELEASE_UPLOAD_DIR)/$(tar_file)" \
		&& echo "Are you sure you have an updated checkout?" \
		&& echo "This should never happen." \
		&& false)
	@echo "Good."

release-remove-old:
	$(RM) $(tar_file) $(sha1_file) $(gpg_file)

release-check: release-verify-even-micro release-verify-newer release-remove-old distcheck

release-upload: release-check $(tar_file) $(sha1_file) $(gpg_file)
	mkdir -p releases
	scp $(tar_file) $(sha1_file) $(gpg_file) $(RELEASE_UPLOAD_HOST):$(RELEASE_UPLOAD_DIR)
	mv $(tar_file) $(sha1_file) $(gpg_file) releases
	git-tag -s -m "compiz $(COMPIZ_VERSION_MAJOR).$(COMPIZ_VERSION_MINOR).$(COMPIZ_VERSION_MICRO) release" compiz-$(COMPIZ_VERSION_MAJOR).$(COMPIZ_VERSION_MINOR).$(COMPIZ_VERSION_MICRO)

release-publish-message: releases/$(sha1_file)
	@echo "Please follow the instructions in RELEASING to push stuff out and"
	@echo "send out the announcement mails.  Here is the excerpt you need:"
	@echo ""
	@echo "Subject: [ANNOUNCE] $(PACKAGE)-$(VERSION)"
	@echo ""
	@echo "============================== CUT HERE =============================="
	@echo "A new $(PACKAGE) release $(VERSION) is now available from:"
	@echo ""
	@echo "	$(RELEASE_URL_BASE)/$(tar_file)"
	@echo ""
	@echo "    which can be verified with:"
	@echo ""
	@echo "	$(RELEASE_URL_BASE)/$(sha1_file)"
	@echo -n "	"
	@cat releases/$(sha1_file)
	@echo ""
	@echo "	$(RELEASE_URL_BASE)/$(gpg_file)"
	@echo "	(signed by `getent passwd "$$USER" | cut -d: -f 5 | cut -d, -f 1`)"
	@echo ""
	@echo "  Additionally, a git clone of the source tree:"
	@echo ""
	@echo "	git clone git://git.freedesktop.org/git/xorg/app/compiz"
	@echo ""
	@echo "    will include a signed $(VERSION) tag which points to a commit named:"
	@echo "	`git cat-file tag compiz-$(VERSION) | grep ^object | sed -e 's,object ,,'`"
	@echo ""
	@echo "    which can be verified with:"
	@echo "	git verify-tag compiz-$(VERSION)"
	@echo ""
	@echo "    and can be checked out with a command such as:"
	@echo "	git checkout -b build compiz-$(VERSION)"
	@echo ""
	@echo "============================== CUT HERE =============================="

release-publish: release-upload release-publish-message

.PHONY: release-verify-even-micro release-verify-newer release-remove-old release-check release-upload release-publish docs-publish
