/*
 * "Ballfield"
 *
 *   (C) David Olofson <david@olofson.net>, 2002, 2003
 *
 * This software is released under the terms of the GPL.
 *
 * Contact author for permission if you want to use this
 * software, or work derived from it, under other terms.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <android/log.h>

#include "SDL.h"
#include "SDL_image.h"

#include "ballfield.h"



/*----------------------------------------------------------
	General tool functions
----------------------------------------------------------*/

/*
 * Bump areas of low and high alpha to 0% or 100%
 * respectively, just in case the graphics contains
 * "alpha noise".
 */
SDL_Surface *clean_alpha(SDL_Surface *s)
{
	SDL_Surface *work;
	SDL_Rect r;
	Uint32 *pixels;
	int pp;
	int x, y;
	
	work = SDL_CreateRGBSurface(SDL_SWSURFACE, s->w, s->h,
			32, 0xff000000, 0x00ff0000, 0x0000ff00,
			0x000000ff);
	if(!work)
		return NULL;

	r.x = r.y = 0;
	r.w = s->w;
	r.h = s->h;
	if(SDL_BlitSurface(s, &r, work, NULL) < 0)
	{
		SDL_FreeSurface(work);
		return NULL;
	}

	SDL_LockSurface(work);
	pixels = (Uint32 *)work->pixels;
	pp = work->pitch / sizeof(Uint32);
	for(y = 0; y < work->h; ++y)
		for(x = 0; x < work->w; ++x)
		{
			Uint32 pix = pixels[y*pp + x];
			switch((pix & 0xff) >> 4)
			{
			  case 0:
				pix = 0x00000000;
			  	break;
			  default:
			  	break;
			  case 15:
				pix |= 0xff;
			  	break;
			}
			pixels[y*pp + x] = pix;
		}
	SDL_UnlockSurface(work);

	return work;
}


/*
 * Load and convert an antialiazed, zoomed set of sprites.
 */
SDL_Surface *load_zoomed(char *name, int alpha)
{
	SDL_Surface *sprites;
	SDL_Surface *temp = IMG_Load(name);
	if(!temp)
		return NULL;

	/*
	sprites = temp;
	SDL_SetAlpha(sprites, 0, 255);
	temp = clean_alpha(sprites);
	SDL_FreeSurface(sprites);
	*/
	if(!temp)
	{
		fprintf(stderr, "Could not clean alpha!\n");
		return NULL;
	}

	if(alpha)
	{
		SDL_SetAlpha(temp, 0, SDL_ALPHA_OPAQUE);
		sprites = SDL_DisplayFormatAlpha(temp);
	}
	else
	{
		SDL_SetColorKey(temp, SDL_SRCCOLORKEY,
				SDL_MapRGB(temp->format, 0, 0, 0));
		sprites = SDL_DisplayFormat(temp);
	}
	SDL_FreeSurface(temp);

	return sprites;
}


void print_num(SDL_Surface *dst, SDL_Surface *font, int x, int y, float value)
{
	char buf[16];
	int val = (int)(value * 10.0);
	int pos, p = 0;
	SDL_Rect from;

	/* Sign */
	if(val < 0)
	{
		buf[p++] = 10;
		val = -val;
	}

	/* Integer part */
	pos = 10000000;
	while(pos > 1)
	{
		int num = val / pos;
		val -= num * pos;
		pos /= 10;
		if(p || num)
			buf[p++] = num;
	}

	/* Decimals */
	if(val / pos)
	{
		buf[p++] = 11;
		while(pos > 0)
		{
			int num = val / pos;
			val -= num * pos;
			pos /= 10;
			buf[p++] = num;
		}
	}

	/* Render! */
	from.y = 0;
	from.w = 7;
	from.h = 10;
	for(pos = 0; pos < p; ++pos)
	{
		SDL_Rect to;
		to.x = x + pos * 7;
		to.y = y;
		from.x = buf[pos] * 7;
		SDL_BlitSurface(font, &from, dst, &to);
	}
}



/*----------------------------------------------------------
	ballfield_t functions
----------------------------------------------------------*/

ballfield_t *ballfield_init(void)
{
	int i;
	ballfield_t *bf = (ballfield_t *)calloc(sizeof(ballfield_t), 1);
	if(!bf)
		return NULL;
	for(i = 0; i < BALLS; ++i)
	{
		bf->points[i].x = rand() % 0x20000;
		bf->points[i].y = rand() % 0x20000;
		bf->points[i].z = 0x20000 * i / BALLS;
		if(rand() % 100 > 80)
			bf->points[i].c = 1;
		else
			bf->points[i].c = 0;
	}
	return bf;
}


void ballfield_free(ballfield_t *bf)
{
	int i;
	for(i = 0; i < COLORS; ++i)
		SDL_FreeSurface(bf->gfx[i]);
}


static int ballfield_init_frames(ballfield_t *bf)
{
	int i, j;
	/*
	 * Set up source rects for all frames
	 */
	bf->frames = (SDL_Rect *)calloc(sizeof(SDL_Rect), bf->gfx[0]->w);
	if(!bf->frames)
	{
		fprintf(stderr, "No memory for frame rects!\n");
		return -1;
	}
	for(j = 0, i = 0; i < bf->gfx[0]->w; ++i)
	{
		bf->frames[i].x = 0;
		bf->frames[i].y = j;
		bf->frames[i].w = bf->gfx[0]->w - i;
		bf->frames[i].h = bf->gfx[0]->w - i;
		j += bf->gfx[0]->w - i;
	}
	return 0;
}


int ballfield_load_gfx(ballfield_t *bf, char *name, unsigned int color)
{
	if(color >= COLORS)
		return -1;

	bf->gfx[color] = load_zoomed(name, bf->use_alpha);
	if(!bf->gfx[color])
		return -2;

	if(!bf->frames)
		return ballfield_init_frames(bf);

	return 0;
}


void ballfield_move(ballfield_t *bf, Sint32 dx, Sint32 dy, Sint32 dz)
{
	int i;
	for(i = 0; i < BALLS; ++i)
	{
		bf->points[i].x += dx;
		bf->points[i].x &= 0x1ffff;
		bf->points[i].y += dy;
		bf->points[i].y &= 0x1ffff;
		bf->points[i].z += dz;
		bf->points[i].z &= 0x1ffff;
	}
}


void ballfield_render(ballfield_t *bf, SDL_Surface *screen)
{
	int i, j, z;

	/* 
	 * Find the ball with the highest Z.
	 */
	z = 0;
	j = 0;
	for(i = 0; i < BALLS; ++i)
	{
		if(bf->points[i].z > z)
		{
			j = i;
			z = bf->points[i].z;
		}
	}

	/* 
	 * Render all balls in back->front order.
	 */
	for(i = 0; i < BALLS; ++i)
	{
		SDL_Rect r;
		int f;
		z = bf->points[j].z;
		z += 50;
		f = ((bf->frames[0].w << 12) + 100000) / z;
		f = bf->frames[0].w - f;
		if(f < 0)
			f = 0;
		else if(f > bf->frames[0].w - 1)
			f = bf->frames[0].w - 1;
		z >>= 7;
		z += 1;
		r.x = (bf->points[j].x - 0x10000) / z;
		r.y = (bf->points[j].y - 0x10000) / z;
		r.x += (screen->w - bf->frames[f].w) >> 1;
		r.y += (screen->h - bf->frames[f].h) >> 1;
		SDL_BlitSurface(bf->gfx[bf->points[j].c],
			&bf->frames[f], screen, &r);
		if(--j < 0)
			j = BALLS - 1;
	}
}



/*----------------------------------------------------------
	Other rendering functions
----------------------------------------------------------*/

/*
 * Draw tiled background image with offset.
 */
void tiled_back(SDL_Surface *back, SDL_Surface *screen, int xo, int yo)
{
	int x, y;
	SDL_Rect r;
	if(xo < 0)
		xo += back->w*(-xo/back->w + 1);
	if(yo < 0)
		yo += back->h*(-yo/back->h + 1);
	xo %= back->w;
	yo %= back->h;
	for(y = -yo; y < screen->h; y += back->h)
		for(x = -xo; x < screen->w; x += back->w)
		{
			r.x = x;
			r.y = y;
			SDL_BlitSurface(back, NULL, screen, &r);
		}
}



/*----------------------------------------------------------
	main()
----------------------------------------------------------*/

int main(int argc, char* argv[])
{
	ballfield_t	*balls;
	SDL_Surface	*screen;
	SDL_Surface	*temp_image;
	SDL_Surface	*back, *logo, *font;
	SDL_Event	event;
	int		bpp = 24,
			flags = SDL_DOUBLEBUF | SDL_SWSURFACE,
			alpha = 1;
	int		x_offs = 0, y_offs = 0;
	long		tick,
			last_tick,
			last_avg_tick;
	double		t = 0;
	float		dt;
	int		i;
	float		fps = 0.0;
	int		fps_count = 0;
	int		fps_start = 0;
	float		x_speed, y_speed, z_speed;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

	atexit(SDL_Quit);

	for(i = 1; i < argc; ++i)
	{
		if(strncmp(argv[i], "-na", 3) == 0)
			alpha = 0;
		else if(strncmp(argv[i], "-nd", 3) == 0)
			flags &= ~SDL_DOUBLEBUF;
		else if(strncmp(argv[i], "-h", 2) == 0)
		{
			flags |= SDL_HWSURFACE;
			flags &= ~SDL_SWSURFACE;
		}
		else if(strncmp(argv[i], "-f", 2) == 0)
			flags |= SDL_FULLSCREEN;
		else
			bpp = atoi(&argv[i][1]);
	}

	screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, bpp, 0 /*flags*/);
	if(!screen)
	{
		fprintf(stderr, "Failed to open screen!\n");
		exit(-1);
	}

	SDL_WM_SetCaption("Ballfield", "Ballfield");
	if(flags & SDL_FULLSCREEN)
		SDL_ShowCursor(0);

	balls = ballfield_init();
	if(!balls)
	{
		fprintf(stderr, "Failed to create ballfield!\n");
		exit(-1);
	}

	/*
	 * Load and prepare balls...
	 */
	balls->use_alpha = alpha;
	if( ballfield_load_gfx(balls, "blueball.png", 0)
				||
			ballfield_load_gfx(balls, "redball.png", 1) )
	{
		fprintf(stderr, "Could not load balls!\n");
		exit(-1);
	}

	/*
	 * Load background image
	 */
	temp_image = IMG_Load("redbluestars.png");
	if(!temp_image)
	{
		fprintf(stderr, "Could not load background!\n");
		exit(-1);
	}
	back = SDL_DisplayFormat(temp_image);
	SDL_FreeSurface(temp_image);

	/*
	 * Load logo
	 */
	temp_image = SDL_LoadBMP("logo.bmp");
	if(!temp_image)
	{
		fprintf(stderr, "Could not load logo!\n");
		exit(-1);
	}
	SDL_SetColorKey(temp_image, SDL_SRCCOLORKEY|SDL_RLEACCEL,
			SDL_MapRGB(temp_image->format, 255, 0, 255));
	logo = SDL_DisplayFormat(temp_image);
	SDL_FreeSurface(temp_image);

	/*
	 * Load font
	 */
	temp_image = SDL_LoadBMP("font7x10.bmp");
	if(!temp_image)
	{
		fprintf(stderr, "Could not load font!\n");
		exit(-1);
	}
	SDL_SetColorKey(temp_image, SDL_SRCCOLORKEY|SDL_RLEACCEL,
			SDL_MapRGB(temp_image->format, 255, 0, 255));
	font = SDL_DisplayFormat(temp_image);
	SDL_FreeSurface(temp_image);

	last_avg_tick = last_tick = SDL_GetTicks();
	
	enum { MAX_POINTERS = 16, PTR_PRESSED = 4 };
	int touchPointers[MAX_POINTERS][5];
	
	memset(touchPointers, 0, sizeof(touchPointers));
	SDL_Joystick * joysticks[MAX_POINTERS+1];
	for(i=0; i<MAX_POINTERS; i++)
		joysticks[i] = SDL_JoystickOpen(i);
	
	while(1)
	{
		SDL_Rect r;

		/* Timing */
		tick = SDL_GetTicks();
		dt = (tick - last_tick) * 0.001f;
		last_tick = tick;
		
		if( bpp == 32 )
			SDL_FillRect(screen, NULL, 0); // Clear alpha channel

		/* Background image */
		tiled_back(back, screen, x_offs>>11, y_offs>>11);

		/* Ballfield */
		ballfield_render(balls, screen);

		/* Logo */
		r.x = 2;
		r.y = 2;
		SDL_BlitSurface(logo, NULL, screen, &r);

		/* FPS counter */
		if(tick > fps_start + 500)
		{
			fps = (float)fps_count * 1000.0 / (tick - fps_start);
			fps_count = 0;
			fps_start = tick;
		}
		print_num(screen, font, screen->w-37, screen->h-12, fps);
		++fps_count;

		for(i=0; i<MAX_POINTERS; i++)
		{
			if( !touchPointers[i][PTR_PRESSED] )
				continue;
			r.x = touchPointers[i][0];
			r.y = touchPointers[i][1];
			r.w = 50 + touchPointers[i][3] / 10;
			r.h = 50 + touchPointers[i][3] / 10;
			r.x -= r.w/2;
			r.y -= r.h/2;
			Uint32 color = touchPointers[i][3] / 5 + 0x7f;
			if( color > 0xff )
				color = 0xff;
			color = color + color * 0x100 + color * 0x10000;
			SDL_FillRect(screen, &r, color);
			print_num(screen, font, r.x, r.y, i+1);
		}
		int mx, my;
		int b = SDL_GetMouseState(&mx, &my);
		Uint32 color = 0xff;
		if( b )
		{
			color = 0;
			if( b & SDL_BUTTON_LEFT )
				color |= 0xff00;
			if( b & SDL_BUTTON_RIGHT )
				color |= 0xff0000;
		}
		r.x = mx;
		r.y = my;
		r.w = 30;
		r.h = 30;
		r.x -= r.w/2;
		r.y -= r.h/2;
		SDL_FillRect(screen, &r, color);

		SDL_Flip(SDL_GetVideoSurface());
		SDL_Event evt;
		while( SDL_PollEvent(&evt) )
		{
			if(evt.type == SDL_KEYUP || evt.type == SDL_KEYDOWN)
			{
				Uint8	*keys = SDL_GetKeyState(&i);
				if(keys[SDLK_ESCAPE])
					return 0;
				__android_log_print(ANDROID_LOG_INFO, "Ballfield", "SDL key event: evt %s state %s key %d RETURN %d mod %d unicode %d", evt.type == SDL_KEYUP ? "UP  " : "DOWN" , evt.key.state == SDL_PRESSED ? "PRESSED " : "RELEASED", (int)evt.key.keysym.sym, int(evt.key.keysym.sym == SDLK_RETURN), (int)evt.key.keysym.mod, (int)evt.key.keysym.unicode);
			}
			if(evt.type == SDL_VIDEORESIZE)
				__android_log_print(ANDROID_LOG_INFO, "Ballfield", "SDL resize event: %d x %d", evt.resize.w, evt.resize.h);
			if(evt.type == SDL_ACTIVEEVENT)
				__android_log_print(ANDROID_LOG_INFO, "Ballfield", "======= SDL active event: gain %d state %d", evt.active.gain, evt.active.state);
			if( evt.type == SDL_ACTIVEEVENT && evt.active.gain == 0 && evt.active.state & SDL_APPACTIVE )
			{
				// We've lost GL context, we are not allowed to do any GFX output here, or app will crash!
				while( 1 )
				{
					SDL_PollEvent(&evt);
					if( evt.type == SDL_ACTIVEEVENT && evt.active.gain && evt.active.state & SDL_APPACTIVE )
					{
						__android_log_print(ANDROID_LOG_INFO, "Ballfield", "======= SDL active event: gain %d state %d", evt.active.gain, evt.active.state);
						SDL_Flip(SDL_GetVideoSurface()); // One SDL_Flip() call is required here to restore OpenGL context
						// Re-load all textures, matrixes and all other GL states if we're in SDL+OpenGL mode
						// Re-load all images to SDL_Texture if we're using it
						// Now we can draw
						break;
					}
					// Process network stuff, maybe play some sounds using SDL_ANDROID_PauseAudioPlayback() / SDL_ANDROID_ResumeAudioPlayback()
					SDL_Delay(300);
					__android_log_print(ANDROID_LOG_INFO, "Ballfield", "Waiting");
				}
			}
			if( evt.type == SDL_JOYAXISMOTION )
			{
				if( evt.jaxis.which == 0 )
					continue;
				int joyid = evt.jaxis.which - 1;
				touchPointers[joyid][evt.jaxis.axis] = evt.jaxis.value;
			}
			if( evt.type == SDL_JOYBUTTONDOWN || evt.type == SDL_JOYBUTTONUP )
			{
				if( evt.jbutton.which == 0 )
					continue;
				int joyid = evt.jbutton.which - 1;
				touchPointers[joyid][PTR_PRESSED] = (evt.jbutton.state == SDL_PRESSED);
			}
		}

		/* Animate */
		x_speed = 500.0 * sin(t * 0.37);
		y_speed = 500.0 * sin(t * 0.53);
		z_speed = 400.0 * sin(t * 0.21);

		ballfield_move(balls, x_speed, y_speed, z_speed);
		x_offs -= x_speed;
		y_offs -= y_speed;

		t += dt;
	}

	ballfield_free(balls);
	SDL_FreeSurface(back);
	SDL_FreeSurface(logo);
	SDL_FreeSurface(font);
	exit(0);
}
