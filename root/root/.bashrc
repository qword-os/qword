export PATH
export HOME=/root
PS1='\[\033[01;32m\]\u@qword\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]\$ '
HISTCONTROL=ignoredups
HISTSIZE=-1
HISTFILESIZE=-1
export TERM=linux
alias ls="ls --color=auto"
alias clear='printf "\e[2J"'

# [[ $PS1 && -f /usr/share/bash-completion/bash_completion ]] && \
#     . /usr/share/bash-completion/bash_completion
