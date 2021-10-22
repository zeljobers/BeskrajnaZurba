#include <stdlib.h>
#include <stdbool.h>
#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#define SQUARE(x) ((x)*(x))
#define UNUSED_VARIABLE(x) ((void)(x))

#define TIMER_ID 0
#define TIMER_INTERVAL 20

/* makroi kontrola */
#define BLANK_KEY ' '
#define ESC_KEY 27

#define RADIUS_OF_CONTROL 223 /* Poluprecnik kontrolisanja oko kocke */
#define MAX_ROOMS_FOR_VIEW 10 /* Za unapred generisanje soba */
#define DIFFICULTY 15 /* Sto je vece to je lakse za igranje, ali jako sporo. */

static bool animation_ongoing = false; 
static int win_size_x = 680;
static int win_size_y = 460;

static void on_keyboard(unsigned char key, int x, int y);
static void on_reshape(int width, int height);
static void on_display(void);
static void on_timer(int value);
static void on_motion(int x, int y);

/* QUEUE OF ROOMS */
typedef struct _room{
	struct _room *next;
	struct _room *previous;
	int translate_room_x;
	int translate_room_y;
} room;

typedef struct _character {
	int target_x;
	int target_y;
	float target_intesitivity;
	
	int old_target_x;
	int old_target_y;

	/* osa oko koje se rotira  */
	float angle;
	float position_x;
	float position_y;
	float position_z;
	GLfloat angle_smooth;

	float center_of_safe_space_x;
	float center_of_safe_space_y;
	float durability;
} character;

extern void draw_hud(int rp, float hp);
extern void draw_hero(void);
extern void draw_barrier(void);
extern void room_destroyer(room* barriers);
extern void init_room_generator(room **barriers);
extern void room_generator(room** barriers, character* hero, int* room_passed);
extern int collision_detection(character *hero);

int room_passed;
int collision_happend = 0;
character hero;
room *barrier_iterator, * barriers;

int main(int argc, char **argv)
{
    /* Inicijalizuje se GLUT. */
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);

    /* Kreira se prozor. */
    glutInitWindowSize(win_size_x, win_size_y);
    glutInitWindowPosition(200, 100);
    glutCreateWindow("BESKRAJNA ZURBA");

    /* Registruju se callback funkcije. */
	glutPassiveMotionFunc(on_motion);
    glutKeyboardFunc(on_keyboard);
    glutReshapeFunc(on_reshape);
    glutDisplayFunc(on_display);

    /* Obavlja se OpenGL inicijalizacija. */
    glClearColor(0, 0, 0, 0);
    glEnable(GL_DEPTH_TEST);

	/* Sejemo seme za pseudo-nasumicno hvatanje podataka */
	srand(time(NULL));
	/* Neke inicijalizacije */
	room_passed = 0; 
	hero.durability = 100;
	/* Sta cini nerad od coveka vidi sta sam stavio kao argument */
	init_room_generator(&barriers);
    /* TODO Ukljucujemo normalizaciju vektora normala */
    /* Program ulazi u glavnu petlju. */
    glutMainLoop();
    return 0;
}

/* Pravi se lista parametara za iscrtavanje kocaka kardinalnosti MAX_ROOMS_FOR_VIEW*/
void init_room_generator(room **barriers){
	int i;
	*barriers = malloc(sizeof(room));
	if(barriers == NULL) exit(1);
	room* temp_barriers = *barriers;
	temp_barriers->previous = NULL;
	temp_barriers->translate_room_x = 0;
	temp_barriers->translate_room_y = 0;
	barrier_iterator = temp_barriers;
	for(i = 1; i < MAX_ROOMS_FOR_VIEW; i++){
		temp_barriers->next = malloc(sizeof(room));
		if(temp_barriers->next == NULL) exit(1);
		temp_barriers->next->previous = temp_barriers;
		temp_barriers = temp_barriers->next;
		/* Odlucujemo lokaciju sobe */
		temp_barriers->translate_room_x = rand() % 3 - 1;
		temp_barriers->translate_room_y = rand() % 3 - 1;
	}
	temp_barriers->next = NULL;
}

void room_generator(room** barriers, character* hero, int* room_passed){
	glTranslatef(0, 0, DIFFICULTY);
	draw_barrier();
	room* temp_barriers = *barriers;
	/* Crta sobe */
	int i;
	float color_by_z_axis = 0;
	float z_axis = - (int)(hero->position_z + DIFFICULTY / 2) / DIFFICULTY - 1;
	for(i = 0; temp_barriers->next != NULL; i++){
		glTranslatef(temp_barriers->translate_room_x * DIFFICULTY / 2,
				 	 temp_barriers->translate_room_y * DIFFICULTY / 2,
					 DIFFICULTY);
		

		if(i >= *room_passed){
			z_axis = - (hero->position_z + DIFFICULTY / 2) / DIFFICULTY - 1;
			/* Koeficijenti spekularne refleksije materijala na osnovu pozicije korisnika i sobe po z-osi */
			color_by_z_axis = (i - z_axis >  5) ? -1 : - (i - z_axis) / 5;
			GLfloat specular_coeffs[] = { color_by_z_axis, color_by_z_axis, color_by_z_axis, 1 };
			/* Podesavaju se parametri materijala. */
			glMaterialfv(GL_FRONT, GL_SPECULAR, specular_coeffs);
		}
		draw_barrier();
		temp_barriers = (temp_barriers->next != NULL) ? temp_barriers->next : temp_barriers;
	}
	/* Dodaje na kraj liste novu sobu kada heroj predje jednu sobu*/
	if(- (int)(hero->position_z + DIFFICULTY / 2) / DIFFICULTY - 1 == *room_passed
	   && temp_barriers->next == NULL ){
		(*room_passed)++;
		
		/* Azuriraju se koordinate centra sigurnog(gde nema kolizije) prostora */
		hero->center_of_safe_space_x += barrier_iterator->translate_room_x * DIFFICULTY / 2;
		hero->center_of_safe_space_y += barrier_iterator->translate_room_y * DIFFICULTY / 2;
		temp_barriers->next = malloc(sizeof(room));
		if(temp_barriers->next == NULL) exit(1);
		temp_barriers = temp_barriers->next;
		temp_barriers->translate_room_x = rand() % 3 - 1;
		temp_barriers->translate_room_y = rand() % 3 - 1;
		   
		temp_barriers->next = NULL;
		barrier_iterator = barrier_iterator->next;
	}
	collision_happend = collision_detection(hero);	
}

/* Detektujemo koliziju pomocu pozicije i velicine kocke i soba 
 * i zatim namestamo posledice 
 * gde ce kocka biti malo odbacena od ivice i skinuto "zdravlje"(izdrzljivosti) kocke. 
 * Ukoliko se udari u zid izmedju soba izgubice 30% izdrzljivosti.
 * Ukoliko se u sobi udari u zid izgubice 0.5% izdrzljivosti.
 */
extern int collision_detection(character *hero){
	int collision = 0;
	if(hero->durability <= 0) return 1; /* Ako je kocka unistena vracamo da se desava kolizija i ne menjamo istrajnost kocke*/
	if(hero->position_x + 0.5 > -hero->center_of_safe_space_x + DIFFICULTY / 2){
		hero->durability -= (abs(hero->position_x + 0.5 - (-hero->center_of_safe_space_x + DIFFICULTY / 2)) < 0.5) ? 0.5 : 30;
		hero->position_x = -hero->center_of_safe_space_x + DIFFICULTY / 2 - 0.55;
		collision = 1;
		
	}else if(hero->position_x - 0.5 < -hero->center_of_safe_space_x - DIFFICULTY / 2){
		hero->durability -= (abs(hero->position_x - 0.5 - (-hero->center_of_safe_space_x - DIFFICULTY / 2)) < 0.5) ? 0.5 : 30;
		hero->position_x = -hero->center_of_safe_space_x - DIFFICULTY / 2 + 0.55;
		collision = 1;
	}
	if(-hero->position_y + 0.5 > hero->center_of_safe_space_y + DIFFICULTY / 2){
		hero->durability -= (abs(-hero->position_y + 0.5 - (hero->center_of_safe_space_y + DIFFICULTY / 2)) < 0.5) ? 0.5 : 30;
		collision = 1;
		hero->position_y = -hero->center_of_safe_space_y - DIFFICULTY / 2 + 0.55;
	}
	else if(- hero->position_y - 0.5 < hero->center_of_safe_space_y - DIFFICULTY / 2){
		hero->durability -= (abs(-hero->position_y - 0.5 - (hero->center_of_safe_space_y - DIFFICULTY / 2)) < 0.5) ? 0.5 : 30;
		hero->durability -= .5;
		collision = 1;
		hero->position_y = -hero->center_of_safe_space_y + DIFFICULTY / 2 - 0.55;
	}
	return collision;
}

/*
 * Otarasavanje od curenja memorije.
 */
void room_destroyer(room* barriers){
	if(barriers == NULL) return;
	room_destroyer(barriers->next);
	free(barriers);
}

/*
 * Uzeto sa www.math.rs/~ivan
 */
static void on_reshape(int width, int height)
{
    /* Podesava se viewport. */
    glViewport(0, 0, width, height);
	win_size_y = height;
	win_size_x = width;
    /* Podesava se projekcija. */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, (float) width / height, 1, 100);
}

/*
 * Osam kocaka formiraju sobu. Namerno nisam koristio skaliranje. Lakse mi bilo.
 */

extern void draw_barrier()
{
	
	/* 2 1 8
	 * 3   7
	 * 4 5 6
	 * kocke rasporedjene */

	/* Za svaku kocku koja je vidljiva kao unutrasnji zid menja se boja zavisno od blizine zida pri igracu */
	/* 1 */
	if(collision_happend){
		GLfloat ambient_coeffs[] = {1, 0.3, 0.3, 1 };
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}else{
		float color_by_xy_axis = (-hero.position_y - hero.center_of_safe_space_y)/(DIFFICULTY/2);
	/* Koeficijenti ambijentalne refleksije materijala. */
		GLfloat ambient_coeffs[] = {(color_by_xy_axis)/3,
									0.3, 
									(1 - color_by_xy_axis)/3, 
									1 };
	/* Podesavaju se parametri materijala. */
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}

	glPushMatrix();
		glTranslatef(0,DIFFICULTY, 0);
		glutSolidCube(DIFFICULTY);
	glPopMatrix();
	/* 2 */
	if(collision_happend){
		GLfloat ambient_coeffs[] = {1, 0.3, 0.3, 1 };
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}else{
	/* Koeficijenti ambijentalne refleksije materijala. */
		GLfloat ambient_coeffs[] = {0,.3,.3, 1};
	/* Podesavaju se parametri materijala. */
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}
	glPushMatrix();
		glTranslatef(DIFFICULTY, 0, 0);
		glTranslatef(0,DIFFICULTY, 0);
		glutSolidCube(DIFFICULTY);
	glPopMatrix();
	/* 3 */
	if(collision_happend){
		GLfloat ambient_coeffs[] = {1, 0.3, 0.3, 1 };
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}else{
		float color_by_xy_axis = (- hero.position_x - hero.center_of_safe_space_x)/(DIFFICULTY/2);
	/* Koeficijenti ambijentalne refleksije materijala. */
		GLfloat ambient_coeffs[] = {color_by_xy_axis/3,
									0.3, 
									(1 - color_by_xy_axis)/3, 
									1 };
	/* Podesavaju se parametri materijala. */
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}
	glPushMatrix();
		glTranslatef(DIFFICULTY, 0, 0);
		glutSolidCube(DIFFICULTY);
	glPopMatrix();
	/* 4 */
	if(collision_happend){
		GLfloat ambient_coeffs[] = {1, 0.3, 0.3, 1 };
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}else{
	/* Koeficijenti ambijentalne refleksije materijala. */
		GLfloat ambient_coeffs[] = {0,.3,.3, 1};
	/* Podesavaju se parametri materijala. */
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}
	glPushMatrix();
		glTranslatef(DIFFICULTY,0, 0);
		glTranslatef(0,-DIFFICULTY, 0);
		glutSolidCube(DIFFICULTY);
	glPopMatrix();
	/* 5 */
	if(collision_happend){
		GLfloat ambient_coeffs[] = {1, 0.3, 0.3, 1 };
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}else{
		float color_by_xy_axis = (-hero.position_y - hero.center_of_safe_space_y)/(DIFFICULTY/2);
	/* Koeficijenti ambijentalne refleksije materijala. */
		GLfloat ambient_coeffs[] = {(1 - color_by_xy_axis)/3,
									0.3, 
									color_by_xy_axis/3, 
									1 };
	/* Podesavaju se parametri materijala. */
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}
	glPushMatrix();
		glTranslatef(0,-DIFFICULTY, 0);
		glutSolidCube(DIFFICULTY);
	glPopMatrix();
	/* 6 */
	if(collision_happend){
		GLfloat ambient_coeffs[] = {1, 0.3, 0.3, 1 };
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}else{
	/* Koeficijenti ambijentalne refleksije materijala. */
		GLfloat ambient_coeffs[] = {0,.3,.3, 1};
	/* Podesavaju se parametri materijala. */
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}
	glPushMatrix();
		glTranslatef(-DIFFICULTY, 0, 0);
		glTranslatef(0,-DIFFICULTY, 0);
		glutSolidCube(DIFFICULTY);
	glPopMatrix();
	/* 7 */
	if(collision_happend){
		GLfloat ambient_coeffs[] = {1, 0.3, 0.3, 1 };
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}else{
		float color_by_xy_axis = (- hero.position_x - hero.center_of_safe_space_x)/(DIFFICULTY/2);
	/* Koeficijenti ambijentalne refleksije materijala. */
		GLfloat ambient_coeffs[] = {(1 - color_by_xy_axis)/3,
									0.3, 
									color_by_xy_axis/3, 
									1 };
	/* Podesavaju se parametri materijala. */
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}
	glPushMatrix();
		glTranslatef(-DIFFICULTY, 0, 0);
		glutSolidCube(DIFFICULTY);
	glPopMatrix();
	/* 8 */
	if(collision_happend){
		GLfloat ambient_coeffs[] = {1, 0.3, 0.3, 1 };
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}else{
	/* Koeficijenti ambijentalne refleksije materijala. */
		GLfloat ambient_coeffs[] = {0,.3,.3, 1};
	/* Podesavaju se parametri materijala. */
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	}
	glPushMatrix();
		glTranslatef(-DIFFICULTY, 0, 0);
		glTranslatef(0,DIFFICULTY, 0);
		glutSolidCube(DIFFICULTY);
	glPopMatrix();
}

/*
 * Ovde se koristi ona realizacija rotacija iz on_timer funkcije i ostalih funkcija.
 * Kreira se kocka u nezavisno koordinatnom sistemu.
 */
extern void draw_hero()
{
    glPushMatrix();
		glDisable(GL_LIGHTING); /* Mali HUD */
		glLineWidth(5);
		glColor3f(1 - hero.durability / 100, hero.durability / 100,0);
		glBegin(GL_LINES);
			glVertex3f(.5 , - 1, 0);
			glVertex3f((100 - hero.durability)/100 - .5 , - 1, 0);
		glEnd();
		glLineWidth(6);
		glColor3f(0.1, 0.1, 0.1);
		glBegin(GL_LINES);
			glVertex3f(.5 , - 1, 0);
			glVertex3f(-.5 , - 1, 0);
		glEnd();
		glEnable(GL_LIGHTING);
		glRotatef( hero.angle_smooth, hero.position_y, -hero.position_x, 0);
		glutSolidCube(1);
    glPopMatrix();
}

static void on_display(void)
{
	/* Uzeto sa www.math.rs/~ivan */
    /* Pozicija svetla (u pitanju je direkcionalno svetlo). */
    GLfloat light0_position[] = {1 , 0.1, -1 , 0 };

    /* Ambijentalna boja svetla. */
    GLfloat light_ambient[] = { 0.3, 0.3, 0.3, 1 };

    /* Difuzna boja svetla. */
    GLfloat light_diffuse[] = { 0.7, 0.7, 0.7, 1 };

    /* Spekularna boja svetla. */
    GLfloat light_specular[] = { 0.9, 0.9, 0.9, 1 };

    /* Koeficijenti ambijentalne refleksije materijala. */
    GLfloat ambient_coeffs[] = { (collision_happend) ? 1 : 0.1, 0.3, 0.3, 1 };

    /* Koeficijenti difuzne refleksije materijala. */
    GLfloat diffuse_coeffs[] = { 0, 1, 1, 1 };

    /* Koeficijenti spekularne refleksije materijala. */
    GLfloat specular_coeffs[] = { 1, 1, 1, 1 };

    /* Koeficijent glatkosti materijala. */
    GLfloat shininess = 20;

    /* Brise se prethodni sadrzaj prozora. */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Podesava se vidna tacka. */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0, -5, 0, 0, 0, 0, 1, 0);

	draw_hud(room_passed, hero.durability);
    /* Ukljucuje se osvjetljenje i podesavaju parametri svetla. */
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);


    /* Podesavaju se parametri materijala. */
    glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse_coeffs);
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular_coeffs);
    glMaterialf(GL_FRONT, GL_SHININESS, shininess);

	/* Inicijalno je tako, ali za svaki slucaj. */
    glShadeModel(GL_SMOOTH); 
 	draw_hero();   

	glTranslatef(hero.position_x, hero.position_y, hero.position_z);
	/* Kreiraju se sobe sa posebnom pozicijom svetlosti. */

    GLfloat light1_position[] = {0.1 , 0.1, -1 , 0 };
    glLightfv(GL_LIGHT0, GL_POSITION, light1_position);
	room_generator(&barriers, &hero, &room_passed);   
	
    /* Nova slika se salje na ekran. */
    glutSwapBuffers();
}

static void on_keyboard(unsigned char key, int x, int y){
	UNUSED_VARIABLE(x);
	UNUSED_VARIABLE(y);
	switch(key){
		case ESC_KEY:
			/* Resava curenje memorije i izlazi iz programa */
			room_destroyer(barriers);
			exit(EXIT_SUCCESS);
		case BLANK_KEY:
			/* Zabranjeno koriscenje pri "smrti" objekta*/
			if(!animation_ongoing && hero.durability > 0){
			/* Zaglavljuje mis na krajevima okruzenja za kontrolu heroja */
				 glutWarpPointer(hero.old_target_x + win_size_x / 2,
		 			  	 - hero.old_target_y + win_size_y / 2);
				glutTimerFunc(TIMER_INTERVAL, on_timer, TIMER_ID);
				animation_ongoing = true;
			}else{
			/* U slucaju pauziranja */
				animation_ongoing = false;
			}
			break;
	}
}

static void on_motion(int x, int y){
	if(!animation_ongoing) return;
	/* Cuvanje starih prilagodjenih koordinata 
	 * kursora za ogranicavanje kontrole kretanja
	 */
	hero.old_target_x = hero.target_x;
	hero.old_target_y = hero.target_y;
	/* Prilagodjavalje koordinate kursora za dalji rad programa*/
	hero.target_x = x - win_size_x / 2;
	hero.target_y = -y + win_size_y / 2;
	/*
	 * Ogranicavanje oblasti za kontrolu kretanja 
	 */
	if(sqrt(SQUARE(hero.target_x) + SQUARE(hero.target_y)) > RADIUS_OF_CONTROL){
		
		 hero.target_x = hero.old_target_x;
		 hero.target_y = hero.old_target_y;
		 glutWarpPointer(hero.old_target_x + win_size_x / 2,
		 			  	 - hero.old_target_y + win_size_y / 2);
		return;
	}else{
	/* Prilagodjavanje brzine rotacije kocke na osnovu udjaljenosti od kocke */
		hero.target_intesitivity = sqrt(SQUARE(hero.target_x) + SQUARE(hero.target_y)) / RADIUS_OF_CONTROL;
		hero.angle += hero.target_intesitivity;
	}
}

static void on_timer(int value){
    /* Proverava se da li callback dolazi od odgovarajuceg tajmera. */
    if (value != TIMER_ID){
        return;
	}
	/*
	 * Realizacija glatkog rotiranja objekta i translacije celog sveta, 
	 * improvizacije kretanja kocke. Sve dok je objekat "ziv" se ovo obavlja,
	 * ali ako nije "ziv" program deformise se lokacija 
	 * dok se ne vratimo skroz nazad na kraj i zaustavlja se program kompletno.
	 */
	hero.angle_smooth += (hero.angle_smooth > hero.angle) ? hero.target_intesitivity : -hero.target_intesitivity;
	if(hero.durability > 0){
		hero.position_z -= sqrt(3 * DIFFICULTY) / DIFFICULTY / 2;
		hero.position_x += (float)hero.target_x / 2 / RADIUS_OF_CONTROL ; 
		hero.position_y -= (float)hero.target_y / 2 / RADIUS_OF_CONTROL ; 
	}else{
		if(hero.position_z < 0){
			hero.position_y = SQUARE(DIFFICULTY);
			hero.position_z += DIFFICULTY;
		}else{
			animation_ongoing = 0;
		}
	}
	/* Forsira se ponovno iscrtavanje prozora. */
	glutPostRedisplay();
	/* Ponovo ukljucujemo timer */
	if(animation_ongoing) glutTimerFunc(TIMER_INTERVAL, on_timer, TIMER_ID);
}

extern void draw_hud(int rp, float hp){
	UNUSED_VARIABLE(hp);
	char *temp_rp = malloc(1<<13U);

	if(NULL == temp_rp) exit(EXIT_FAILURE);
	sprintf(temp_rp, "[%d]", rp);
	glDisable(GL_LIGHTING);
	int i = 0;
	glColor3f(0,0,1);	
	glRasterPos3f(0.5,-0.5, -2.5); /* Crvena knjiga */
	for(i = 0; temp_rp[i] != '\0'; i++){
 		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, temp_rp[i]);
	}
	glColor3f(0, 1, 0);
	glRasterPos3f(1, -0, -2.5);
	if(!animation_ongoing){
		if(hero.durability > 0){
			sprintf(temp_rp, "PRETISNI SPACE");
			for(i = 0; temp_rp[i] != '\0'; i++){
				glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, temp_rp[i]);
			}
		}else{
			sprintf(temp_rp, "If you want to try again press ESC button and execute");
			for(i = 0; temp_rp[i] != '\0'; i++){
				glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, temp_rp[i]);
			}
		}
	}
	free(temp_rp);
	glEnable(GL_LIGHTING);
}

