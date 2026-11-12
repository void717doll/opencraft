#ifndef CLIENTMATERIALPALETTE_HPP_
#define CLIENTMATERIALPALETTE_HPP_
#include <SDL.h>
#include "common/core/Exception.hpp"
#include "common/core/MaterialPalette.hpp"

inline void applyMaterialPalette(SDL_Surface *surface,const std::string &filename) {
	auto kind=materialPalette::forTexture(filename);
	if (kind==materialPalette::Kind::None) return;
	if (surface->format->BytesPerPixel!=4 || SDL_LockSurface(surface)!=0)
		throw EXCEPTION("unable to read material texture:",filename);
	for (int y=0;y<surface->h;++y) {
		auto *row=reinterpret_cast<Uint32 *>(static_cast<u8 *>(surface->pixels)+y*surface->pitch);
		for (int x=0;x<surface->w;++x) {
			u8 r,g,b,a;
			SDL_GetRGBA(row[x],surface->format,&r,&g,&b,&a);
			if (!a) continue;
			auto color=materialPalette::color(kind,r,g,b);
			row[x]=SDL_MapRGBA(surface->format,color[0],color[1],color[2],a);
		}
	}
	SDL_UnlockSurface(surface);
}
#endif
