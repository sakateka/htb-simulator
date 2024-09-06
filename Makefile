default: help

IMPL := tb-old
OUTPUT := out-pipe
SCRIPT := scripts/example.sh
BINARY := simulator
CFLAGS := -Wall -Werror -O3 -g -lm

.PHONY: help
help: ## Optional vars IMPL=<tb implementation> OUTPUT=<pipe for metrics output> SCRIPT=<scenario script>
	@sed -nr 's/^([^:]+:)[^#]+## (.*)$$/\\e[1;32m\1\\e[0m \2\\n/p' Makefile|sort|xargs -d '\n' -n1 printf

.PHONY: $(IMPL) clean run script
$(IMPL): ## Build the simulator for the $(IMPL) token bucket implementation
	clang $(CFLAGS) -o $(BINARY) $(IMPL).c

clean:
	rm -vf $(BINARY) $(OUTPUT)

$(OUTPUT):
	mkfifo $(OUTPUT)

run: $(IMPL) $(OUTPUT)  ## Run the REPL simulator (default IMPL=tb-old)
	@printf "\e[1;32mPlease\e[0m run 'tlook $(CURDIR)/$(OUTPUT)' in another console\n"
	stdbuf -o0 rlwrap -m -M.sh -pgreen -S'=> ' -b' ' -fcompletion.txt -e '' ./$(BINARY) > $(OUTPUT)

script: $(IMPL) $(OUTPUT)  ## Run the simulator with a script (default SCRIPT=scripts/example.sh IMPL=tb-old)
	@printf "\e[1;32mPlease\e[0m run 'tlook $(CURDIR)/$(OUTPUT)' in another console\n"
	stdbuf -o0 ./$(BINARY) < $(SCRIPT) > $(OUTPUT)
