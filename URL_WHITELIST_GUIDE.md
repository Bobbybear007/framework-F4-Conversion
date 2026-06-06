# PrismaUI_F4 URL Whitelist Configuration

## Overview

The PrismaUI framework blocks all external URLs by default for security. Use the whitelist to allow specific trusted domains (wikis, CDNs, asset servers) to load images, scripts, and styles.

## How It Works

**Three-layer security:**

1. **JS API Kill** — fetch, XMLHttpRequest, WebSocket blocked at JavaScript level
2. **Content-Security-Policy (CSP)** — Browser-level enforcement blocks remaining network attempts
3. **Domain Whitelist** — Specific domains get exceptions in CSP directives

## Adding Whitelisted Domains

### Step 1: Edit URLWhitelist.h

File: `src/PrismaUI/URLWhitelist.h`

```cpp
static constexpr std::array<std::string_view, 3> WHITELISTED_DOMAINS = {
    "static.wikia.nocookie.net",    // Fallout wiki images
    "cdn.jsdelivr.net",              // JavaScript CDN
    "fonts.googleapis.com",           // Google Fonts
    // Add new domains here:
    "your-domain.com",
    "api.example.com",
};
```

### Step 2: Rebuild PrismaUI_F4

```bash
cd E:\F4SE OG\Prisma\PrismaUI_F4 New Gen
cmake --preset default
cmake --build build
```

### Step 3: Deploy

The new whitelist is automatically included in the CSP meta tag injected into all views.

## Example: Allow Fallout Wiki Images

**Current whitelisting:**
```cpp
"static.wikia.nocookie.net",  // ✓ Already whitelisted
```

**Result in CSP:**
```
img-src 'self' data: blob: file: https://static.wikia.nocookie.net ...
```

**Your auction house can now load:**
```html
<img src="https://static.wikia.nocookie.net/fallout/images/5/51/Laser_rifle.png">
```

## Supported Resource Types

Whitelisted domains can load:

- **Images** — `img-src` directive
- **Scripts** — `script-src` directive (embedded/inline only)
- **Styles** — `style-src` directive
- **Fonts** — `font-src` directive

JavaScript from whitelisted domains cannot execute due to `'unsafe-eval'` being absent. Only inline scripts are allowed.

## Security Notes

- **Only add trusted domains** — Whitelisting is equivalent to giving that domain code execution in the game UI
- **Use HTTPS only** — All whitelisted URLs must use `https://` (http:// is blocked)
- **Subdomain support** — Whitelist `example.com` to automatically allow:
  - `example.com`
  - `www.example.com`
  - `api.example.com`
  - `cdn.example.com`
  - `game.example.com` (e.g., `fallout4.nexusmods.com`)
- **No wildcard support** — Exact domain matches only (but subdomains covered)

### Nexus Mods Subdomain Examples
By whitelisting `nexusmods.com`, these all work:
- `nexusmods.com` ✓
- `www.nexusmods.com` ✓
- `fallout4.nexusmods.com` ✓ (Fallout 4)
- `skyrimspecialedition.nexusmods.com` ✓ (Skyrim SE)
- `starfield.nexusmods.com` ✓ (Starfield)
- `oblivion.nexusmods.com` ✓ (Oblivion)

## Testing

After rebuilding and deploying:

1. Open a view that loads whitelisted resources
2. Check browser console (F12) for CSP violations
3. If images load, whitelist is working

**CSP violation example (if domain NOT whitelisted):**
```
Refused to load the image 'https://blocked-domain.com/image.png' 
because it violates the following Content-Security-Policy directive: 
"img-src 'self' data: blob: file: https://static.wikia.nocookie.net ..."
```

## Current Whitelist (as of 2026-06-04)

- `static.wikia.nocookie.net` — Fallout wiki images
- `cdn.jsdelivr.net` — JavaScript libraries, Tailwind CSS CDN
- `fonts.googleapis.com` — Google Fonts
- `youtube.com` — YouTube videos (www.youtube.com, youtube.com, etc.)
- `youtu.be` — YouTube short links (youtu.be/ABC123)
- `nexusmods.com` — Nexus Mods (any game: fallout4.nexusmods.com, skyrim.nexusmods.com, etc.)

## Common Domains to Add

| Domain | Purpose | Add If | Example |
|--------|---------|--------|---------|
| `api.github.com` | GitHub CDN | Using GitHub-hosted assets | Release notes, images |
| `unpkg.com` | NPM CDN | Using unpkg for libraries | React, Vue, etc. |
| `cdnjs.cloudflare.com` | Cloudflare CDN | Using Cloudflare libraries | jQuery, Bootstrap |
| `your-server.com` | Your own server | Hosting custom assets | Game data, UI themes |

## Troubleshooting

**Images not loading?**
- Check CSP violation in F12 console
- Verify domain is in WHITELISTED_DOMAINS array
- Rebuild and redeploy PrismaUI_F4
- Clear browser cache

**Getting "net::ERR_BLOCKED_BY_CLIENT"?**
- Domain is not whitelisted
- Add it to URLWhitelist.h and rebuild

**Performance issues?**
- Whitelisted domains still respect normal network rules
- Use a CDN for best performance
- Static assets should use `https://` with long cache headers

## Code Examples

### Fallout Wiki Images
```html
<!-- Whitelisted: static.wikia.nocookie.net -->
<img src="https://static.wikia.nocookie.net/fallout/images/5/51/Laser_rifle.png" 
     alt="Laser Rifle">
```

### YouTube Videos
```html
<!-- Embedded YouTube video (whitelisted: youtube.com) -->
<iframe src="https://www.youtube.com/embed/dQw4w9WgXcQ" 
        width="560" height="315" 
        allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture">
</iframe>

<!-- YouTube short link -->
<a href="https://youtu.be/dQw4w9WgXcQ">Watch Video</a>
```

### Nexus Mods

#### Mod Page Images
```html
<!-- Display mod thumbnail from Nexus Mods (whitelisted: nexusmods.com) -->
<img src="https://staticdelivery.nexusmods.com/mods/110/images/105620/105620-1234567890-1234567890.png" 
     alt="Mod Preview">
```

#### Mod Links
```html
<!-- Links to any Nexus Mods game/mod (all supported via subdomain matching) -->

<!-- Fallout 4 Mods -->
<a href="https://www.nexusmods.com/fallout4/mods/105620">View Mod on Nexus</a>

<!-- Skyrim Mods -->
<a href="https://www.nexusmods.com/skyrimspecialedition/mods/12345">Skyrim SE Mod</a>

<!-- Starfield Mods -->
<a href="https://www.nexusmods.com/starfield/mods/54321">Starfield Mod</a>
```

#### Nexus Mods API (Future Enhancement)
```javascript
// Once connected to Nexus API, can fetch mod data:
// fetch('https://api.nexusmods.com/v1/games/fallout4/mods/105620')
// NOTE: API calls still blocked by connect-src 'none' 
// To enable, would need to whitelist specific API endpoints separately
```

### Non-Whitelisted Domain (Blocked)
```html
<!-- This will be blocked (not in whitelist) -->
<img src="https://example.com/weapon.png" alt="Weapon">
```

**To enable example.com:**
1. Edit `URLWhitelist.h`
2. Add `"example.com"` to WHITELISTED_DOMAINS
3. Rebuild PrismaUI_F4

## References

- CSP Specification: https://developer.mozilla.org/en-US/docs/Web/HTTP/CSP
- URLWhitelist.h: `src/PrismaUI/URLWhitelist.h`
- NetworkSandbox.h: `src/PrismaUI/NetworkSandbox.h`
