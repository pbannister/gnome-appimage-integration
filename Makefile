#
#	Makefile for the GNOME AppImage Integration project.
#
#	This Makefile is the human-facing driver.
#	Each rule calls the appropriate tool instead of reimplementing work:
#		build:  sh scripts/program-build.sh (readers and command-line tools)
#		        The build tree is written to dataflow.out/build/.
#		site:   sh scripts/site-build.sh + sh scripts/site-condense.sh
#		        (the standard page set; see prompts/features/02-project-pages.md)
#		clean:  remove generated output, including the path-bound build tree
#		test:   npm test
#		deploy: RETIRED - all publishing to labs.bannister.us goes through
#		        the homelab project (homelab-publish; see the homelab's
#		        documents/09-project-pages-conventions.md). This target
#		        only reminds you of that; use the homelab's make deploy.
#		install: sh scripts/program-install.sh (prefix defaults to $HOME/.local)
#		release: sh scripts/release-package.sh (the files a GitHub release publishes)
#		release-publish: sh scripts/release-publish.sh (needs the GitHub CLI)
#

build:
	sh scripts/program-build.sh

site:
	sh scripts/site-build.sh
	sh scripts/site-condense.sh

clean:
	rm -rf dataflow.out/build dataflow.out/release dataflow.out/generated
	rm -f dataflow.out/* site.out/* logs/*

test:
	npm test

deploy:
	@echo '==== RETIRED: publishing goes through the homelab project (homelab-publish).'
	@echo '==== Build the pages here (make site), then run the homelab''s "make deploy"'
	@echo '==== to publish (see homelab documents/09-project-pages-conventions.md).'

install:
	sh scripts/program-install.sh

release:
	sh scripts/release-package.sh

release-publish:
	sh scripts/release-publish.sh

.PHONY: build site clean test deploy install release release-publish
