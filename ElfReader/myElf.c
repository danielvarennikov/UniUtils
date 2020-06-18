#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <elf.h>

/*Flags*/
char debug_mode = 0;
int Currentfd = -1;
char* map_start = NULL;
int filesize;

/*Name of function, the function itself*/
struct fun_desc {

    char *name;
    void (*fun)();

};

/*Flip the debug switch*/
void toggle_debug() {

    if(debug_mode == 0){

        debug_mode = 1;
        printf("Debug flag now on\n");

    }else{

        debug_mode = 0;
        printf("Debug flag now off\n");

    }

}

/*Print the main ELF Info*/
void examine_elf(){

    char input_buff[100];
    struct stat st;

    /*Check if we have any file previously opened -> Close it*/
    if(Currentfd != -1)
        close(Currentfd);

    /*Get the filename from the user*/
    printf("Please enter the filename:\n");
    fgets(input_buff ,100 ,stdin);

    /*Remove \n from filename*/
    input_buff[strlen(input_buff) - 1] = '\0';

    /*Open the file for reading*/
    Currentfd = open(input_buff, O_RDONLY);

    /*No file exists in directory -> return*/
    if(Currentfd <= 0){

        printf("No file with matching filename exists in directory\n");
        return;

    }

    /*Get the size of the file*/
    stat(input_buff, &st);

    filesize = st.st_size;

    /*Map to memory*/
    map_start = (char *)mmap(NULL, st.st_size, PROT_READ,MAP_PRIVATE, Currentfd, 0);

    /*Check that our file is ELF*/
    if(map_start[1] != 69 || map_start[2] != 76 || map_start [3] != 70){
        printf("Not an ELF file!\n");
        return;
    }

    /*Print all the information*/
    Elf32_Ehdr* Elf32_ptr = NULL;

    /*Pointer to the beginning of our mapped file*/
    Elf32_ptr = (Elf32_Ehdr *)map_start;

    printf("==================== ELF HEADER ==================================\n");
    printf("The magic numbers: %c%c%c\n", Elf32_ptr->e_ident[1],Elf32_ptr->e_ident[2],Elf32_ptr->e_ident[3]);
    printf("The Encoding scheme: %d\n", Elf32_ptr->e_ident[4]);
    printf("The Entry point: %p\n", (void *)Elf32_ptr->e_entry);
    printf("Start of section header: %ld\n", (unsigned long)Elf32_ptr->e_shoff);
    printf("Number of section header entries: %d\n", Elf32_ptr->e_shnum);
    printf("Size of section headers: %d\n", Elf32_ptr->e_shentsize);
    printf("Start of program headers: %ld\n", (unsigned long)Elf32_ptr->e_phoff);
    printf("Number of program headers: %d\n", Elf32_ptr->e_phnum);
    printf("Size of program headers: %d\n", Elf32_ptr->e_phentsize);
    printf("==================== ELF HEADER ==================================\n");

}

/*Get a pointer to the string table of the prog*/
char* get_section_names(){

    /*Point to the beginning of the map*/
    Elf32_Ehdr *Elf32_ptr = (Elf32_Ehdr* )map_start;

    /*The sections amount*/
    int sec_num = Elf32_ptr->e_shnum;

    /*Point to the sections section*/
    Elf32_Shdr* shdr_ptr = (Elf32_Shdr *)(map_start + Elf32_ptr->e_shoff);

    /*Point to the string table*/
    char *name_ptr= NULL;

    /*Get to .shstrtab section -> Holds section names*/
    while (sec_num--) {

        /*The string table of the section?*/
        if (shdr_ptr->sh_type == SHT_STRTAB) {

            name_ptr = (char *)(map_start + shdr_ptr->sh_offset);

            /*Finish when we point to .shstrtab -> No confusion with .dynstr*/
            if (!strcmp(&name_ptr[shdr_ptr->sh_name], ".shstrtab"))
                break;

        }

        /*Next section*/
        shdr_ptr++;

    }

    return name_ptr;

}

/*readelf -S*/
void print_sections(){

    /*Rudamentary checks*/
    if(Currentfd <= 0 || map_start == NULL){

        printf("No file mapped yet!\n");
        return;

    }

    /*Sections header pointer*/
    Elf32_Shdr* shdr_ptr = NULL;

    /*Point to the beginning of the map*/
    Elf32_Ehdr *Elf32_ptr = (Elf32_Ehdr* )map_start;

    /*Point to the string table*/
    char *name_ptr = get_section_names();

    /*Point to symbols section*/
    shdr_ptr = (Elf32_Shdr *)(map_start + Elf32_ptr->e_shoff);

    /*The sections amount*/
    int sec_num = Elf32_ptr->e_shnum;
    int i = 0;
	
    printf("Section headers:\n");

    /*Print them all*/
    while (sec_num--) {
        printf("[%02d]%s type: 0x%x, offset: 0x%x, size: 0x%x\n", i++,
               &name_ptr[shdr_ptr->sh_name], shdr_ptr->sh_type,
               shdr_ptr->sh_offset, shdr_ptr->sh_size);

        /*Next section*/
        shdr_ptr++;
    }

    printf("\n");
}

/*readelf -s*/
void print_symbols(){

    /*Rudamentary checks*/
    if(Currentfd <= 0 || map_start == NULL){

        printf("No file mapped yet!\n");
        return;

    }

    /*Point to the start of the file*/
    Elf32_Ehdr *Elf32_ptr = (Elf32_Ehdr *)map_start;

    /*Point to the sections section(Rolling)*/
    Elf32_Shdr *shdr_ptr = (Elf32_Shdr *)(map_start + Elf32_ptr->e_shoff);

    /*Yet another pointer...(Stable)*/
    Elf32_Shdr *section = (Elf32_Shdr*)(map_start+Elf32_ptr->e_shoff);

    /*Get the amount of sections*/
    int n = Elf32_ptr->e_shnum;
	
    printf(".symtab:\n");    

    while (n--) {

        /*Get to the symbol table*/
        if (shdr_ptr->sh_type == SHT_SYMTAB) {

            /*Get the amount of symbols*/
            int sym_amount = (shdr_ptr->sh_size / sizeof(Elf32_Sym));
            int sym_index = 0;

            /*Point to the first symbol(Rolling)*/
            Elf32_Sym *sym = (Elf32_Sym *)(map_start + shdr_ptr->sh_offset);

            /*Use for dynamic symbol printing(Stable)*/
            Elf32_Sym *dyn_sym = (Elf32_Sym *) (map_start + shdr_ptr->sh_offset);

            /*The dynamic symbol names offset*/
            char *symbol_names = (char *) (map_start + (section + shdr_ptr->sh_link)->sh_offset);

            /*symtab[sym_index].st_name -> the index of the symbol in the symbol name table*/
            while (sym_amount--) {

                printf("[%02d] value: 0x%x size: %d index: %d name: %s\n",
                        sym_index, sym->st_value,sym->st_size,sym->st_shndx,symbol_names + dyn_sym[sym_index].st_name);

                sym_index++;
                sym++;
            }
        }

        /*Next section*/
        shdr_ptr++;
    }

}

/*readelf -r*/
void relocs_table(){

    /*Rudamentary checks*/
    if(Currentfd <= 0 || map_start == NULL){

        printf("No file mapped yet!\n");
        return;

    }

    Elf32_Ehdr *Elf32_ptr = (Elf32_Ehdr *)map_start;

    /*Point to string table adress*/
    char *name_ptr = get_section_names();

    /*Point to the sections table*/
    Elf32_Shdr *shdr_ptr = (Elf32_Shdr *)(map_start + Elf32_ptr->e_shoff);

    /*Get the number of section headers*/
    int sec_num = Elf32_ptr->e_shnum;

    /*Yet another pointer...(Stable)*/
    Elf32_Shdr *section = (Elf32_Shdr*)(map_start+Elf32_ptr->e_shoff);

    /*The dynamic symbol names offset*/
    char *symbol_names;

    /*symtab[sym_index].st_name -> the index of the dynamic symbol in the symbol name table*/
    Elf32_Dyn *dyntab;

    /*Find the location of the symbols table -> used for .dynstr afterwards*/
    int n = sec_num;
    while (n--) {

        /*Get to the dynamic symbol table*/
        if (shdr_ptr->sh_type == SHT_DYNSYM) {

            /*Point to the first symbol*/
            dyntab = (Elf32_Dyn *) (map_start + shdr_ptr->sh_offset);

            /*The dynamic symbol names offset*/
            symbol_names = (char *) (map_start + (section + shdr_ptr->sh_link)->sh_offset);
            break;
        }

        /*Next section*/
        shdr_ptr++;
    }

    int i = 0;
    
    /*Point to the sections table*/
    shdr_ptr = (Elf32_Shdr *)(map_start + Elf32_ptr->e_shoff);

    /*Iterate until we find SHT_REL or SHT_RELA*/
    while (sec_num--) {

        /*
        	SHT_RELA -> The section holds relocation entries.
                SHT_REL -> The section holds a relocation entries without explicit addends.
        */
        if (shdr_ptr->sh_type == SHT_REL || shdr_ptr->sh_type == SHT_RELA) {

            printf("[%02d]%s type: 0x%x, offset: 0x%x, size: 0x%x\n", i++,
                   &name_ptr[shdr_ptr->sh_name], shdr_ptr->sh_type,
                   shdr_ptr->sh_offset, shdr_ptr->sh_size);

            /*Get the amount of entries in the current section*/
            int amount_entries = (shdr_ptr->sh_size / sizeof(Elf32_Rel));

            /*Beginning of the section*/
            Elf32_Rel *rel = (Elf32_Rel *)(map_start + shdr_ptr->sh_offset);

            /*Print all the entries in the section*/
            while (amount_entries--) {
                int dyn_index = ELF32_R_SYM(rel->r_info) * 2;

                printf("offset: 0x%x, size: 0x%x, info: 0x%x, name: %s, type: %d\n",
                       rel->r_offset, sizeof(Elf32_Rel),rel->r_info,symbol_names + dyntab[dyn_index].d_tag, ELF32_R_TYPE(rel ->r_info));

                /*Next entry*/
                rel++;
            }

            printf("\n");

        }

        /*Next section*/
        shdr_ptr++;
    }

}

/*Exit program*/
void quit(){

    if(debug_mode == 1)
        printf("Qitting...\n");

    if(Currentfd != -1)
        close(Currentfd);

    /*Unmap from virtual memmry*/
    munmap(map_start, filesize);

    exit(0);

}

int main(int argc, char **argv){

    /*An array of all our functions with their descriptions*/
    struct fun_desc menu[] = { {"Toggle Debug Mode", toggle_debug },
                               {"Examine ELF File",examine_elf},
                               {"Print Section Names",print_sections},
                               {"Print Symbols",print_symbols},
                               {"Relocation Tables",relocs_table},
                               {"Quit", quit},
                               {NULL, NULL} };


    /*The character the user chooses*/
    char chosen;

    int bounds = (sizeof(menu)/sizeof(menu[1]) - 1) - 1;

    while(1){

        printf("Please choose a function:\n");

        int i = 0;
        while(1){

            if(menu[i].name == NULL)
                break;
            else
                printf("%d) %s\n",i,menu[i].name);
            i++;
        }

        /*Get the chosen option from the user*/
        printf("Option: ");

        chosen = fgetc(stdin) - '0';

        /*Clears the input buffer*/
        while ((fgetc(stdin)) != '\n');

        printf("\n");

        /*Check if the chosen option is within our bounds*/
        if(chosen < 0 || chosen > bounds){
            printf("Not within bounds\n");
            exit(0);
        }

        menu[(int)chosen].fun();


    }

    return 0;
}

