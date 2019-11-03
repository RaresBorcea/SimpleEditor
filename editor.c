#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/select.h>

#define TRUE 1
#define BUFMAX 1005 // dimensiunea maxima a unei linii
#define FILENAME 30 // dimensiunea numelui fisierului
#define GOTONO 6 // numarul de cifre al numerelor din comenzile GOTO/DL

// Definirea structurii liniilor
typedef struct line {
	char letter;
	struct line* next;
	struct line* prev;
}*Line;

// Definitia structurii textului (liste la fiecare linie)
typedef struct list {
	Line line;
	struct list* next;
	struct list* prev;
}*List;

// Definirea structurii nodului unei stive
typedef struct node {
	int type; // tipul comenzii
	int pozix, poziy; // coordonatele inaintea comenzii
	int pozfx, pozfy; // coordonatele dupa introducerea comenzii
	int no; // numarul de caractere modificate/adaugate
	int del; // numarul liniei sterse de dl (variabila utilizata si in replace)
	Line text1, text2; // textul introdus, modificat, inlocuit
	struct node* next;
}*Node;

// Definitia structurii de date pentru stive
typedef struct stack {
	Node top;
	int size;
}*Stack;

int xc = 0, yc = 0; // coordonatele cursorului
int xb = 0, yb = 0; // pozitia anterioara a cursorului
List text = NULL; // textul editat
Stack comm = NULL; // stiva de comenzi
Stack undostack = NULL; // stiva undo

/*
 * Functii generale, pentru manipularea structurilor
 */
// initializarea unui nod (litera) in linie
Line initLine(char letter) {
	Line new;
	new = (Line) malloc (sizeof(struct line));
	new->letter = letter;
	new->next = NULL;
	new->prev = NULL;
	return new;
}

// initializarea unui nod (linie) in lista principala
List initList(Line line) {
	List new;
	new = (List) malloc (sizeof(struct list));
	new->line = line;
	new->next = NULL;
	new->prev = NULL;
	return new;
}

// stergerea listei principale
List deleteList(List l) {
	List temp = l, p;
	while(temp != NULL) {
		p = temp->next;
		free(temp);
		temp = p;
	}
	l = NULL;
	return l;
}

// initializarea nodurilor pentru stivele commands/undo
Node initNode(int type, int pozix, int poziy, int pozfx, 
	int pozfy, int no, int del, Line text1, Line text2) 
{
	Node new = (Node) malloc(sizeof(struct node));
	new->type = type;
	new->pozix = pozix;
	new->poziy = poziy;
	new->pozfx = pozfx;
	new->pozfy = pozfy;
	new->no = no;
	new->del = del;
	new->text1 = text1;
	new->text2 = text2;
	new->next = NULL;
	return new;
}

// stergerea nodurilor din stive
Node freeNode(Node node) {
	Node temp = node, p;
	while(temp != NULL) {
		p = temp->next;
		free(temp);
		temp = p;
	}
	return NULL;
}

// initializarea stivelor commmands/undo
Stack initStack(int type, int pozix, int poziy, int pozfx, 
	int pozfy, int no, int del, Line text1, Line text2) {
	Stack stack = (Stack) malloc(sizeof(struct stack));
	stack->top = initNode(type, pozix, poziy, pozfx, pozfy, no, del, text1, text2);
	stack->size = 1;
	return stack;
}

// verificare daca stiva e vida
int isEmptyStack(Stack stack) {
	if(stack == NULL || stack->size == 0) {
		return 1;
	} else {
		return 0;
	}
}

// adaugarea unui nod la stiva
Stack push(Stack stack, int type, int pozix, int poziy, int pozfx, 
	int pozfy, int no, int del, Line text1, Line text2) {
	if(isEmptyStack(stack)) {
		stack = initStack(type, pozix, poziy, pozfx, pozfy, no, del, text1, text2);
	} else {
		Node new = initNode(type, pozix, poziy, pozfx, pozfy, no, del, text1, text2);
		new->next = stack->top;
		stack->top = new;
		stack->size++;
	}
	return stack;
}

// stergerea ultimului nod din stiva
Stack pop(Stack stack) {
	if(isEmptyStack(stack)) {
		return NULL;
	} else {
		Node temp = stack->top;
		stack->top = stack->top->next;
		stack->size--;
		free(temp);
		return stack;
	}
}

// duplicarea ultimului nod din stiva
Node top(Stack stack) {
	if(isEmptyStack(stack)) {
		return NULL;
	} else {
		int type = stack->top->type;
		int pozix = stack->top->pozix;
		int poziy = stack->top->poziy;
		int pozfx = stack->top->pozfx;
		int pozfy = stack->top->pozfy;
		int no = stack->top->no;
		int del = stack->top->del;
		Line text1 = stack->top->text1;
		Line text2 = stack->top->text2;
		Node new = initNode(type, pozix, poziy, pozfx, pozfy, no, del, text1, text2);
		return new;
	}
}

// stergerea stivei
Stack freeStack(Stack stack) {
	stack->top = freeNode(stack->top);
	stack = NULL;
	return stack;
}

/*
 * Functiile necesare manipularii textului
 */
// transpunerea sirurilor de caractere in linii ale listei principale
Line lineMaker(char *buffer) {
	int i;
	Line line = initLine(buffer[0]), new, temp = line;
	for(i = 1; i < strlen(buffer); i++) {
		new = initLine(buffer[i]);
		temp->next = new;
		new->prev = temp;
		temp = temp->next;
	}
	return line;
}

// adaugarea la/crearea listei principale (intregul text) pe baza liniilor (randurilor)
void textMaker(Line line) {
	if(text == NULL)
	{
		text = initList(line);
		xc = 0; yc = 2;
	} else {

		int j = 1;
		int g; // sunt pe o linie noua nealocata sau nu
		List tempt = text;
		while(j < yc - 1) { // merg pana la randul anterior pozitiei curente
				tempt = tempt->next;
				j++;
		}

		// verific starea randului urmator (propriu-zis)
		if((tempt->next == NULL && yc != 1) || (tempt == NULL)) {
			g = 0; // sunt la inceputul unei linii noi
		} else {
			g = 1; // sunt pe o linie alocata
		}

		// analizez toate variantele de introducere a randurilor
		if(xc == 0 && g == 0) { // rand nou/linie vida
			int i = 1;
			tempt = text; Line templ;
			while(tempt->next != NULL) {
				tempt = tempt->next;
				i++;
			}

			templ = tempt->line;
			while(templ->next != NULL) {
				templ = templ->next;
			}

			templ->next = line;
			line->prev = templ;
			List new = initList(line);
			tempt->next = new;
			new->prev = tempt;
			i += 2;
			yc = i;
		} else if(xc == 0 && g == 1 && yc != 1) {
			// rand alocat, scrierea la inceputul acestuia si mutarea pe randul urmator
			Line templ, p;
			templ = tempt->line;
			while(templ->letter != '\n') {
				templ = templ->next;
			}

			p = templ->next;
			templ->next = line;
			line->prev = templ;
			templ = line;
			while(templ->next != NULL) {
				templ = templ->next;
			}

			templ->next = p;
			if(p != NULL) {
				p->prev = templ;
			}

			List new = initList(line);
			new->next = tempt->next;
			tempt->next->prev = new;
			tempt->next = new;
			new->prev = tempt;
			yc++;
		} else if(xc == 1 && g == 1 && yc != 1) {
			// rand alocat, scrierea dupa primul caracter al acestuia
			Line templ, p;
			templ = tempt->line;
			while(templ->letter != '\n') {
				templ = templ->next;
			}

			templ = templ->next;
			p = templ->next;
			templ->next = line;
			line->prev = templ;
			templ = line;
			while(templ->next != NULL) {
				templ = templ->next;
			}

			templ->next = p;
			if(p != NULL) {
				p->prev = templ;
			}

			tempt = tempt->next;
			List new = initList(p);
			new->next = tempt->next;
			tempt->next->prev = new;
			tempt->next = new;
			new->prev = tempt;
			yc++;
		} else if(xc == 0 && g == 1 && yc == 1) {
			// scrierea la inceputul primului rand, acesta fiind deja alocat
			Line templ, p;
			p = tempt->line;
			templ = line;
			while(templ->next != NULL) {
				templ = templ->next;
			}

			templ->next = p;
			if(p != NULL) {
				p->prev = templ;
			}

			List new = initList(line);
			new->next = tempt;
			tempt->prev = new;
			text = new;
			yc++;
		} else if(xc == 1 && g == 1 && yc == 1) {
			// scrierea dupa primul caracter de pe primul rand (alocat)
			Line templ, p;
			p = tempt->line;
			p = p->next;
			templ = line;
			while(templ->next != NULL) {
				templ = templ->next;
			}

			templ->next = p;
			if(p != NULL) {
				p->prev = templ;
			}

			List new = initList(p);
			new->next = text->next;
			text->next->prev = new;
			text->next = new;
			new->prev = text;
			tempt->line->next = line;
			line->prev = tempt->line;
			yc++;
		} else if(xc != 1 && xc != 0) {
			// toate celelalte situatii
			Line templ, p;
			if(yc != 1) {
				// se ajunge la randul curent (*)
				templ = tempt->next->line;
			} else {
				templ = tempt->line;
			}

			int j = 1;
			while(j < xc) {
				// ne pozitionam pe caracterul curent
				templ = templ->next;
				j++;
			}

			p = templ->next;
			templ->next = line;
			line->prev = templ;
			templ = line;
			while(templ->next != NULL) {
				templ = templ->next;
			}
			templ->next = p;
			if(p != NULL) {
				p->prev = templ;
			}

			List new = initList(p);
			if(yc == 1) {
				// discutarea celor doua cazuri pe baza (*) de mai sus
				new->next = tempt->next;
				if(tempt->next != NULL) {
					tempt->next->prev = new;
				}
				new->prev = tempt;
				tempt->next = new;
			} else {
				new->next = tempt->next->next;
				if(tempt->next->next != NULL) {
					tempt->next->next->prev = new;
				}
				new->prev = tempt->next;
				tempt->next->next = new;
			}
			yc++;
			xc = 0;
		}
	}
}

// functie auxiliara de inserare a textului intr-o pozitie data (pentru undo si redo)
void insert(int type, int xins, int yins, int no, Line ins) {
	int i = 1, g = 0; // inseram linie noua
	xins++; // pornim de la zero pentru situatia folosirii anterioare a Go to line
	List temp = text, prev = NULL, first;
	Line line, p = NULL, q;
	while(i < yins) {
		//ne pozitionam pe randul curent
		prev = temp;
		temp = temp->next;
		i++;
	}

	if(temp != NULL) {
		// daca acesta nu este vid
		line = temp->line;
		q = line;
	}
	if(temp == NULL) {
		// daca e vid, il alocam
		temp = initList(ins);
		if(xins == 1 && yins == 0) {
			// daca e inceputul textului, pointam spre noul rand
			text = temp; 
		}
		first = temp;
		temp->prev = prev;
		if(prev != NULL) {
			// modific legaturile dintre randuri
			prev->next = temp;
			Line leg = prev->line;
			while(leg->letter != '\n')
				leg = leg->next;
			leg->next = ins;
			ins->prev = leg;
		}
		g = 1; // s-a inserat o linie noua
	} else {
		i = 1;
		while(i < xins) {
			// ne pozitionam pe caracterul curent
			q = line;
			line = line->next;
			i++;
		}
		p = line; // modific legaturile dintre caractere
		if(p != NULL) {
			q = p->prev;
		}
		ins->prev = q;
		if(q != NULL) {
			q->next = ins;
		}
	}
	
	i = 1;
	int j = 1;
	line = ins;
	List startins;

	// caz special de introducere de text in interiorul unui rand alocat
	if(type == 9 && p != NULL && g == 0 && xins != 1) {
		List new9 = initList(p);
		if(temp->next != NULL) {
			temp->next->prev = new9;
		}
		new9->next = temp->next;
		temp->next = new9;		
		new9->prev = temp;
	}

	while(i < no) {
		if(line->letter == '\n') {
			// daca intalnesc newline si mai exista caractere, trec pe rand nou
			if(line->next != NULL && (i + 1) < no) {
				// primul rand nou se introduce inaintea celui curent
				if(j == 1 && g == 0 && xins == 1) {
					List new = initList(line->next);
					new->next = temp;
					if(temp->prev != NULL) {
						new->prev = temp->prev;
						temp->prev->next = new;
					}
					temp->prev = new;
					temp = new;
					startins = temp;
				} else {
					// celelalte se introduc in continuarea primului
					List new = initList(line->next);
					if(temp->next != NULL) {
						temp->next->prev = new;
					}
					new->next = temp->next;
					temp->next = new;
					new->prev = temp;
					temp = temp->next;
				}
				j++;
			}
		}
		line = line->next;
		i++;
	}

	if(!g) {
		// finalizez modificarea legaturilor dintre caractere
		line->next = p;
		if(p != NULL) {
			p->prev = line;
		}
	}

	// finalizez modificarea legaturilor dinte randuri, pe cazuri
	// in functie de pozitii si de tipul functiilor anulate/reefectuate
	if(xins == 1 && yins == 1 && g == 0) {
		List new = initList(ins);
		new->next = text;
		text->prev = new;
		text = new;
		// (*) tipurile numerice ale funciilor se regasesc in Readme
		if(line->letter != '\n' && type == 16) {
			text->next = text->next->next;
			if(text->next != NULL) {
				temp->next->prev = text;
			}
		}
	} else if(xins == 1 && yins == 1 && g == 1) {
		text = first;
	} else if(xins == 1 && yins != 1 && g == 0) {
		if(j == 1 || j == 2) {
			List new = initList(ins);
			new->next = temp;
			temp->prev->next = new;
			new->prev = temp->prev;
			temp->prev = new;
			if(line->letter != '\n' && type == 16) {
				new->next = temp->next;
				temp->next->prev = new;
			}
			if(line->letter != '\n' && type != 16) {
				if(temp->next->next != NULL) {
					temp->next->next->prev = temp;
				}
				temp->next = temp->next->next;
			}
		} else {
			List new = initList(ins);
			new->next = startins;
			startins->prev->next = new;
			new->prev = startins->prev;
			startins->prev = new;
			if(line->letter != '\n') {
				if(temp->next->next != NULL) {
					temp->next->next->prev = temp;
				}
				temp->next = temp->next->next;
			}
		}
				
	} else if(xins != 1 && yins != 1 && type == 16 && ins->letter == '\n') {
		List new = initList(p);
		new->next = temp->next;
		if(temp->next != NULL) {
			temp->next->prev = new;
		}
		new->prev = temp;
		temp->next = new;
	} else if(xins != 1 && yins == 1 && type == 16 && ins->letter == '\n') {
		List new = initList(ins);
		new->next = temp;
		if(temp != NULL) {
			temp->prev = new;
		}
		new->prev = temp->prev;
		text = new;
	} else if(xins != 1 && type == 12 && ins->letter == '\n') {
		List new = initList(ins->next);
		new->next = temp->next;
		if(temp->next != NULL) {
			temp->next->prev = new;
		}
		temp->next = new;
		new->prev = temp;
	}
}

/*
 * Functiile editorului de text
 */
// functia de salvare in fisierul text, returneaza 1 la succes
int save(char *file) {	
	FILE *dest = fopen(file, "w");
	if(!dest) {
		printf("Nu am putut deschide fisierul");
		return 0;
	}
	
	if(text != NULL) {
		Line line = text->line;
		while(line != NULL) {
			char c = line->letter;
			fputc(c, dest);
			line = line->next;
		}
	}
	fclose(dest);
	return 1;
}

// functia backspace
void back() {
	// verific daca aplicarea backspace e posibila
	if((xc == 0 && yc == 0) || (xc == 0 && yc == 1)) {
		return ;
	} else if (xc == 0 && yc != 0 && yc != 1) {
		// analizez cazurile posibile
		// inceput de rand -> unirea cu randul anterior
		List temp = text;
		int j = 1;
		while(j < yc - 1) {
				temp = temp->next;
				j++;
		}
		Line line = temp->line, p;

		j = 1;
		while(line->letter != '\n') {
			line = line->next;
			j++;
		}

		xb = 0; yb = yc; // modificarea coordonatelor cu pastrarea celor initiale
		xc = j - 1;
		yc--;
		line = line->prev;
		p = line->next;
		line->next = line->next->next;
		if(line->next != NULL) {
			line->next->prev = line;
		}
		if(temp->next != NULL) {
			if(temp->next->next != NULL) {
				temp->next->next->prev = temp;
				temp->next = temp->next->next;
			} else {
				temp->next = deleteList(temp->next);
			}	
		}

		// adaugarea comenzii in stiva commands
		comm = push(comm, 12, xb, yb, xc, yc, 1, 0, p, NULL); 
	} else if(xc == 1 && yc == 1) {
		// stergerea primului caracter de pe primul rand
		Line p = text->line;
		text->line = text->line->next;
		xb = 1; yb = 1;
		xc = 0; yc = 0;
		comm = push(comm, 12, xb, yb, xc, yc, 1, 0, p, NULL);
	} else {
		// celelalte situatii
		List temp = text;
		Line p, line;
		int j = 1;
		while(j < yc) {
			temp = temp->next;
			j++;
		}

		j = 1;
		line = temp->line;
		while(j < xc) {
			line = line->next;
			j++;
		}

		p = line;
		if(line->next != NULL) {
			line->next->prev = line->prev;
		}
		line->prev->next = line->next;
		xb = xc; yb = yc;
		xc--;
		comm = push(comm, 12, xb, yb, xc, yc, 1, 0, p, NULL);
	}
}

// functia Go to line
void gl(char *buffer) {
	int i = 0, j = 0, num;
	char number[GOTONO];
	while(!isdigit(buffer[i])) {
		// extragerea parametrului numeric
		i++;
	}

	for(i = i; i < strlen(buffer); i++) {
		number[j] = buffer[i];
		j++;
	}
	number[j] = 0;
	num = atoi(number); // conversia in tipul int
	xb = xc; yb = yc;
	xc = 0;
	yc = num;
	// adaugarea comenzii in stiva commands
	comm = push(comm, 14, xb, yb, xc, yc, 0, 0, NULL, NULL);
}

// functia Go to char
void gc(char *buffer) {
	int i = 0, j = 0, numc;
	char numchar[GOTONO];
	while(!isdigit(buffer[i])) {
		// extragerea parametrului numeric 1
		i++;
	}
	for(i = i; buffer[i] != ' ' && i < strlen(buffer); i++) {
		numchar[j] = buffer[i];
		j++;
	}

	numchar[j] = 0;
	numc = atoi(numchar);
	xb = xc; yb = yc;
	if(buffer[i] == ' ') {
		// extragerea parametrului numeric 2, daca exista
		int numl;
		i++;
		j = 0;
		char numline[GOTONO];
		for(i = i; i < strlen(buffer); i++) {
			numline[j] = buffer[i];
			j++;
		}
		numline[j] = 0;
		numl = atoi(numline);
		yc = numl;
	}
	xc = numc - 1;
	// adaugarea comenzii in stiva commands
	comm = push(comm, 15, xb, yb, xc, yc, 0, 0, NULL, NULL);
}

// functia Delete line
void dl(char *buffer) {
	int i = 0, j;
	// extragerea parametrului numeric, daca exista
	while(!isdigit(buffer[i]) && i <= strlen(buffer)) {
		i++;
	}
	if(i - 1 != strlen(buffer)) {
		j = 0;
		int num;
		char number[GOTONO];
		for(i = i; i < strlen(buffer); i++) {
			number[j] = buffer[i];
			j++;
		}
		number[j] = 0;
		num = atoi(number);
		List temp = text;

		// verificare daca linia este noua nealocata -> nu efectuam delete
		j = 1;
		while(j < num - 1) {
			temp = temp->next;
			j++;
		}
		if((temp->next == NULL && num != 1) || (temp == NULL && num == 1)) {
			return ;
		}

		if(num != 1) {
			// ajungem pe linia curenta
			temp = temp->next;
		}
		Line line = temp->line, p;

		j = 1;
		while(line->letter != '\n') {
			// o parcurgem pana la sfarsit si refacem legaturile
			line = line->next;
			j++;
		}
		p = temp->line->prev;
		if(num != 1) {
			p->next = line->next;
		}
		if(line->next != NULL) {
			// daca mai exista text dupa
			line->next->prev = p;
			xb = xc; yb = yc;
			if(num == yc) {
				xc = 1;
			} else if(num < yc) {
				yc--;
			}
			// adaugarea comenzii in stiva commands
			comm = push(comm, 13, xb, yb, xc, yc, j, num, temp->line, NULL);
			temp->next->prev = temp->prev;
			if(num != 1) {
				temp->prev->next = temp->next;
			}

		} else {
			xb = xc; yb = yc;
			if(num == yc) {
				xc = 0;
			} else if(num < yc) {
				yc--;
			}
			// adaugarea comenzii in stiva commands
			comm = push(comm, 13, xb, yb, xc, yc, j, num, temp->line, NULL);
			if(num != 1) {
				temp->prev->next = temp->next;
			}
		} if(num == 1) {
			// daca stergem linia 1, trecem pe linia 2
			text = text->next;
		}
	} else {
		// daca nu exista parametru numeric, stergem linia curenta

		// modificari analoage cazului I
		List temp = text;

		// verificare daca linia este noua nealocata
		j = 1;
		while(j < yc - 1) {
			temp = temp->next;
			j++;
		}
		if((temp->next == NULL && yc != 1 && yc != 0) 
			|| (temp == NULL && (yc == 1 || yc == 0))) {
			return ;
		}

		if(yc != 1) {
			temp = temp->next;
		}
		Line line = temp->line, p;
		j = 1;
		while(line->letter != '\n' && line->next != NULL) {
			line = line->next;
			j++;
		}
		p = temp->line->prev;
		if(yc != 1) {
			p->next = line->next;
		}
		if(line->next != NULL) {
			line->next->prev = p;
			xb = xc; yb = yc;
			xc = 1;
			comm = push(comm, 13, xb, yb, xc, yc, j, yc, temp->line, NULL);
			temp->next->prev = temp->prev;
			if(yc != 1) {
				temp->prev->next = temp->next;
			}
		} else {
			xb = xc; yb = yc;
			xc = 0;
			comm = push(comm, 13, xb, yb, xc, yc, j, yc, temp->line, NULL);
			if(yc != 1) {
				temp->prev->next = temp->next;
			}
		}

		if(yc == 1) {
			text = text->next;
		}
	}
}

// functia Delete
// mode == 0 -> delete explicit
// mode == 1 -> delete/undo al textului tocmai introdus
void d(char *buffer, int mode) {
	int i = 0, j, num;
	// extragerea parametrului numeric, daca exista
	while(!isdigit(buffer[i]) && i <= strlen(buffer)) {
		i++;
	}
	if(i - 1 != strlen(buffer)) {
		j = 0;
		char number[GOTONO];
		for(i = i; i < strlen(buffer); i++) {
			number[j] = buffer[i];
			j++;
		}
		number[j] = 0;
		num = atoi(number);
	} else {
		num = 1; // altfel stergem 1 caracter
	}
	List temp = text, q;

	// verificare daca linia este noua nealocata
	j = 1;
	while(j < yc - 1) {
		temp = temp->next;
		j++;
	}
	if((temp->next == NULL && yc != 1 && yc != 0) 
		|| (temp == NULL && (yc == 1 || yc == 0))) {
			return ;
		}

	if(yc != 1 && yc != 0) {
		// ajung pe linia curenta
		temp = temp->next;
	}
	Line line = temp->line, p;
	i = 0;
	while(i < xc) {
		// ajung la caracterul curent
		line = line->next;
		i++;
	}
	i = 1;
	p = line;
	int g = 0; // retin daca se sterge ultima linie 
	while(i <= num) {
		// modific legaturile dintre randuri daca sterg peste sfarsitul de rand
		if(line->letter == '\n' && line->next != NULL) {
			//mai exista text dupa
			q = NULL;
			if(temp->next != NULL) {
				q = temp->next->next;
			}
			temp->next = q;
			if(q != NULL) {
				q->prev = temp;
			}
		} else if(line->letter == '\n' && line->next == NULL) {
			// sterg pana la final
			if(xc != 0) {
				// din interiorul randului
				q = NULL;
				if(temp->next != NULL) {
					q = temp->next->next;
				}
				temp->next = q;
				if(q != NULL) {
					q->prev = temp;
				}
			} else {
				// de la inceputul sau -> randul dispare
				if(temp->prev != NULL) {
					temp->prev->next = temp->next;
				}
				g = 1; // deci se sterge ultima linie
			}
			
		}
		line = line->next;
		i++;
	}

	// modific legaturile dintre caractere, pe cazuri
	if(line != NULL) {
		line->prev = p->prev;
	}
	if(p->prev != NULL) {
		p->prev->next = line;
	}
	i = 0;
	if(xc == 0 && yc == 1) {
		// sterg de la primul caracter din text
		while(i < num) {
			text->line = text->line->next;
			i++;
		}
	} else if(xc == 0 && yc != 1 && g == 0) {
		// sterg de la primul caracter din rand
		while(i < num) {
			temp->line = temp->line->next;
			i++;
		}
	} else if(xc == 0 && yc == 0) {
		// am ajuns pe randul ''zero'' -> sterg tot
		text = deleteList(text);
	}

	if(!mode) {
		// daca apelul e explicit, adaug comanda la stiva commands
		xb = xc; yb = yc;
		comm = push(comm, 16, xb, yb, xc, yc, num, 0, p, NULL);
	}
}

// functia Quit, folosim biblioteca 'sys/select.h'
void q() {
	system("pkill editor");
}

// functia Replace
// mode == 0 -> replace explicit
// mode == 1 -> folosit pentru undo replace
// mode == 2 -> folosit pentru redo replace
void replace(char *buffer, int mode) {
	int i = 0, j = 0;
	int ramas;
	char oldw[BUFMAX], neww[BUFMAX]; // textul inlocuit si cel nou
	Line start;
	while(!isspace(buffer[i])) {
		// extragerea celor doua din sirul text al comenzii
		i++;
	}
	i++;
	for(i = i; buffer[i] != ' ' && i < strlen(buffer); i++) {
		oldw[j] = buffer[i];
		j++;
	}
	oldw[j] = 0; // adaugarea terminatorului de sir

	i++;
	j = 0;
	int num;
	if(mode) {
		num = strlen(buffer);
	} else {
		num = strlen(buffer) - 1;
	}
	for(i = i; i < num; i++) {
		neww[j] = buffer[i];
		j++;
	}
	neww[j] = 0;

	i = 1;
	List temp = text;
	while(i < yc) {
		temp = temp->next;
		i++;
	}
	i = 0;
	Line line = temp->line;
	while(i < xc) {
		line = line->next;
		i++;
	}
	int g = 0; // nu am gasit cuvantul
	int newline = 0; // inlocuirea presupune modificarea inceputului de rand
	while(line != NULL && g == 0) {
		// pana gasim prima litera din cuvant
		while(line->letter != oldw[0] && line != NULL) {
			if(line->letter == '\n') {
				// mutam cursorul imaginar pe randul urmator
				temp = temp->next;
				newline = 0;
			} else {
				newline++;
			}
					
			line = line->next;
		}

		i = 0;
		// retinem in ce pozitie am gasit prima litera
		start = line;
		ramas = newline; 
		while(g == 0 && line != NULL && i < strlen(oldw)) {
			if(line->letter != oldw[i]) {
				if(line->letter == '\n') {
					temp = temp->next;
					newline = 0;
				}
				break;
			} else {
				i++;
				newline++;
				line = line->next;
				if(i == strlen(oldw)) {
					g = 1; // am gasit tot cuvantul
				}
			}
		}
		if(!g) {
			if(line != NULL) {
				line = line->next;
			}
		}
	}
	if(g == 0) {
		printf("Nu mai exista aparitii ale cuvantului\n");
	} else {	
		// daca l-am gasit, efectuam inlocuirea
		Line p, q;
		p = start;
		Line replword = lineMaker(neww); // cream cuvantului structura tip linie
		if(start->prev != NULL) {
			// refacem legaturile dintre caractere
			start->prev->next = replword;
		}
		replword->prev = start->prev;
		q = replword;
		while(replword->next != NULL) {
			replword = replword->next;
		}
		replword->next = line;
		if(line != NULL) {
			line->prev = replword;
		}
		if(ramas == 0) {
			// daca apare la inceput de rand, modificam pointerul randului
			temp->line = q;
		}
		if(!mode || mode == 2) {
			// adaugam comanda la stiva de comenzi, pentru apelul explicit si redo
			comm = push(comm, 17, xc, yc, xc, yc, strlen(oldw), strlen(neww), p, q);
		}
	}
}

// functia Undo
void undo() {
	if(comm == NULL) {
		// daca nu mai exista comenzi, nu facem undo
		return ;
	}
	Node last = top(comm); // extragem ultimul nod

	// analizam pe tipuri de functii
	if(last->type == 9) {
		// undo ultimul text introdus
		char untype[BUFMAX];
		sprintf(untype, "d %d", last->no); // utilizam Delete pentru undo text
		xc = last->pozix;
		yc = last->poziy;
		d(untype, 1); // in mode == 1, fara adaugare in stiva commands
	}

	if(last->type == 12) {
		// undo backspace
		// analizam separat cazul de backspace newline
		if(last->text1->letter != '\n') {
			insert(last->type, last->pozix - 1, last->poziy, last->no, last->text1);
			xc = last->pozix;
			yc = last->poziy;
		} else {
			insert(last->type, last->pozfx, last->pozfy, last->no, last->text1);
			xc = last->pozix;
			yc = last->poziy;
		}
	}

	if(last->type == 13) {
		// undo delete line
		insert(last->type, 0, last->del, last->no, last->text1);
		xc = last->pozix;
		yc = last->poziy;
	}

	if(last->type == 14) {
		// undo goto line
		xc = last->pozix;
		yc = last->poziy;
	}

	if(last->type == 15) {
		// undo goto char
		xc = last->pozix;
		yc = last->poziy;
	}

	if(last->type == 16) {
		// undo delete char
		insert(last->type, last->pozix, last->poziy, last->no, last->text1);
		xc = last->pozix;
		yc = last->poziy;
	}

	if(last->type == 17) {
		// undo replace
		// undo replace se face prin replace-ul noului cuvant cu cel vechi
		Line oldword = last->text2, newword = last->text1;
		int i = 0;
		char oldw[BUFMAX], neww[BUFMAX], buffer[BUFMAX];
		while(i < last->del) {
			oldw[i] = oldword->letter;
			i++;
			oldword = oldword->next;
		}
		oldw[i] = 0;
		i = 0;
		while(i < last->no) {
			neww[i] = newword->letter;
			i++;
			newword = newword->next;
		}
		neww[i] = 0;
		sprintf(buffer, "re %s %s", oldw, neww); // cream comanda text a replace
		replace(buffer, 1);
	}

	int type = last->type, pozix = last->pozix, poziy = last->poziy, 
		pozfx = last->pozfx, pozfy = last->pozfy, no = last->no, del = last->del;
	Line text1 = last->text1, text2 = last->text2;
	
	// mutarea comenzii din stiva commands in stiva undo (pentru redo)
	undostack = push(undostack, type, pozix, poziy, pozfx, pozfy, no, del, text1, text2);
	comm = pop(comm);
	
	if(comm->size == 0) {
		// stergerea stivei commands
		comm = freeStack(comm);
	}
}

// functia Redo
void redo() {
	if(undostack == NULL) {
		return ;
	}
	Node last = top(undostack);

	if(last->type == 9) {
		// redo pentru ultimul text anulat
		insert(last->type, last->pozix, last->poziy, last->no, last->text1);
		xc = last->pozfx;
		yc = last->pozfy;
		xb = last->pozix;
		yb = last->poziy;
		comm = push(comm, 9, xb, yb, xc, yc, last->no, 0, last->text1, NULL);
	}

	if(last->type == 12) {
		// redo backspace
		xc = last->pozix;
		yc = last->poziy;
		back();
	}

	if(last->type == 13) {
		// redo delete line
		char redl[BUFMAX];
		sprintf(redl, "dl %d", last->del);
		xc = last->pozix;
		yc = last->poziy;
		dl(redl);
	}

	if(last->type == 14) {
		// redo goto line
		char regl[BUFMAX];
		sprintf(regl, "gl %d", last->pozfy);
		xc = last->pozix;
		yc = last->poziy;
		gl(regl);
	}

	if(last->type == 15) {
		// redo goto char
		char regc[BUFMAX];
		sprintf(regc, "gc %d %d", last->pozfx + 1, last->pozfy);
		xc = last->pozix;
		yc = last->poziy;
		gc(regc);
	}

	if(last->type == 16) {
		// redo delete char
		char redel[BUFMAX];
		sprintf(redel, "d %d", last->no);
		xc = last->pozix;
		yc = last->poziy;
		d(redel, 0);
	}

	if(last->type == 17) {
		// redo replace
		Line oldword = last->text1, newword = last->text2;
		int i = 0;
		char oldw[BUFMAX], neww[BUFMAX], buffer[BUFMAX];
		while(i < last->no) {
			oldw[i] = oldword->letter;
			i++;
			oldword = oldword->next;
		}
		oldw[i] = 0;
		i = 0;
		while(i < last->del) {
			neww[i] = newword->letter;
			i++;
			newword = newword->next;
		}
		neww[i] = 0;
		sprintf(buffer, "re %s %s", oldw, neww);
		replace(buffer, 2);
	}

	undostack = pop(undostack);
	
	if(undostack->size == 0) {
		undostack = freeStack(undostack);
	}
}

// functia driver
int main(int argc, char const *argv[]) {
	char file[FILENAME]; // numele fisierului
	int commno = 0; // numarul de comenzi introduse
	int no = 0; // numarul de caractere introduse de la ultima comanda
	char buffer[BUFMAX]; // comanda introdusa
	Line line = NULL, start = NULL; // prima linie de text introdusa
	int g = 1; // retinam daca e prima linie text introdusa

	// mode == 0 -> introducere text
	// mode == 1 -> introducere comenzi
	int mode = 0;

	strcpy(file, argv[1]); // retinem numele fisierului
	while(TRUE) {
		if(fgets(buffer , BUFMAX, stdin) != NULL) {
			if(buffer[0] == ':' && buffer[1] == ':') {
				// daca se introduc '::', se trece de la un mod la altul
				g = 0;
				if(mode == 0) {
					mode = 1;
					// adaugam textul introdus in stiva commands
					comm = push(comm, 9, xb, yb, xc, yc, no, 0, start, NULL);
				} else {
					mode = 0;
					xb = xc; // retinem de unde am inceput introducerea
					yb = yc;
					no = 0;
					g = 1;
				}
				continue;
			}

			if(mode == 0) {
				// introducere text
				no = no + strlen(buffer);
				line = lineMaker(buffer);
				textMaker(line); // cream textul cu ajutorul functiilor
				if(g) {
					start = line; // retinem primul caracter introdus
					g = 0;
				}
			}
			if(mode == 1) {
				// introducere comenzi
				switch(buffer[0]) {
					case 's': if(!save(file)) return 0;
							  break;
					case 'b': back(); break;
					case 'g': if(buffer[1] == 'l') gl(buffer);
							  else if(buffer[1] == 'c') gc(buffer);
							  break;
					case 'd': if(buffer[1] == 'l') dl(buffer);
							  else if(buffer[1] != 'l') d(buffer, 0);
							  break;
					case 'q': q(); break;
					case 'u': undo(); break;
					case 'r': if(buffer[1] != 'e') redo();
							  else if(buffer[1] == 'e') replace(buffer, 0);
							  break;
				}	

				// apelul lui save la fiecare 5 comenzi introduse
				if(buffer[0] != 's') {
					commno++;
					if(commno % 5 == 0) {
						if(!save(file)) {
							return 0;
						}
					}
				}
			}
		}
	}

	return 0;
}
