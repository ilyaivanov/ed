set nocompatible

set scrolloff=8 
set number 
set relativenumber


set tabstop=2 
set softtabstop=2 
set shiftwidth=2 
set expandtab 
set smartindent 
set ignorecase

call plug#begin('~/.vim/plugged') 
Plug 'junegunn/fzf', { 'do': { -> fzf#install() } }
Plug 'junegunn/fzf.vim'

Plug 'ayu-theme/ayu-vim'
Plug 'tpope/vim-commentary'
Plug 'kana/vim-textobj-user'
Plug 'kana/vim-textobj-entire'
call plug#end()

set termguicolors     " enable true colors support
"let ayucolor="light"  " for light version of theme
""let ayucolor="mirage" " for mirage version of theme
let ayucolor="dark"   " for dark version of theme
colorscheme ayu

let mapleader = " "
nnoremap <leader>pv :Vex<CR>
nnoremap <leader>w :w<CR>\
nnoremap <leader>vv <C-v>
nnoremap <leader>n :noh<CR>
nnoremap <leader>r :silent update \| !misc\\build.bat<CR>
"nnoremap <leader>f :silent update \| :silent !clang-format -i -style="{AllowShortFunctionsOnASingleLine:None, ColumnLimit: 120}" %<CR>
nnoremap <leader>f :silent update \| :silent !clang-format --style="{AllowShortFunctionsOnASingleLine: \"Empty\", ColumnLimit: 100}" -i %<CR>


nnoremap <C-p> :Files<CR>
nnoremap <leader>pf :GFiles<CR>

nnoremap <leader>sw :execute 's/' . expand('<cword>') . '/'<Left>
"nnoremap <leader>vb :vsplit C:\\Users\ila_i\\AppData\\Local\\nvim\\init.vim<CR><C-w>h

"nnoremap <leader>so :source C:\\Users\ila_i\\AppData\\Local\\nvim\\init.vim<CR>

nnoremap <CR> i<CR><Esc>

