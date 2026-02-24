          ---------------- Wizball - By Graham Goring ----------------
           ---------------- Trevor Storey & Infamous ----------------
                                  Version 1.00

       This game is in no way endorsed or approved by Sensible Software.



         ---------------- End User License Disagreement ----------------

If this game in any way borks your machine or causes anything bad to occur then
Retrospec and Smila accept no liability. By the mere act of downloading it you
have indemnified us against any future legal action.

In fact, even if we came around to your house and did a big poo on the doorstep,
you couldn't touch us because you downloaded Wizball. That's how legally
watertight and binding this statement it.

I amazed that people ever bother hiring lawyers, this EULA stuff is a piece of
cake...



      ---------------- The Bare Minimum I Can Ask Of You ----------------

The only thing that might be a problem with this game is its reliance on OpenGL.
Many PCs come with woefully out of date OpenGL drivers and so if the game
crashes upon startup you should consider upgrading yours. This game makes use of
OpenGL 1.3, which means it should run on anything from the time of the GeForce 1
or 2 onwards. Anything older than this like a TNT or Matrox card or a stinky
on-board graphics adapter will probably fall over. Considering that a low-level
GeForce 6 will probably cost you about £30-40, you might want to think about
upgrading. Unless of course you've recently upgraded to Vista, in which case you
might want to thing about downgrading to XP Pro. Golly, I wonder if this even
runs on Vista?

If it does crash, then you could try opening up the config.txt file and changing
the line "#OPENGL_13 = TRUE" to "#OPENGL_13 = FALSE". If it still crashes after
that, then "Dude! You got a Dell!".



 ---------------- Your Mission, Should You Choose To Accept It ----------------

Hello! And welcome to the third thing I've programmed while at Retrospec. It's
also the last thing because I'm fed up to the back teeth with making remakes,
and I really hope that attitude shines through in every aspect of this remake
of Wizball. ;)

Although Wizball is primarily a single player game, you can also start a two
player game which remaps all of Catellite's controls to separate keys so that a
loved-one can control them while you get on with the meat of the gameplay,
occasionally pausing to scream at them for not picking up that extra-life
droplet that a paintball left behind.

This game is completely based on the Commodore 64 version of the game.
Originally I was planning on supporting a switch to allow you to play both the
C64 and Spectrum versions of the game, but this was dropped because there's such
a difference in terms of map data that I couldn't be arsed. Also, as much as I
love the ZX Spectrum, the C64 version of Wizball utterly trounces the Spectrum
version in terms of features and gameplay. For a start it's not bugged to
buggery and back.

Although this is based on the C64 version there are some subtle differences in
terms of enemy behaviour and the basic metrics of the game, but hopefully it's
pretty spot-on. Oh, but I added a button to choose bonuses in addition to the
usual joystick-waggle method because a bunch of puss- sorry, beta testers, asked
for it.

Unlike the home-computer versions the enemies won't spawn in odd places so much,
and there's also no limit on the number of enemies on screen so you have to do
less sweeping back and forth to hunt for the last few on each level. There is a
choke limit on the total number of bullets on-screen at one time, however, as
without it things get stupidly hairy later on.

Here's the original instructions from the Spectrum version for your edification:

        For many years, Wiz and his magical cat lived happily in brightly
        coloured Wizworld. All was not well however as a malevolent force
        had discovered the vista and intended to stamp out brilliance
        once and for all. The evil Zark and his horrible sprites have
        moved in to eliminate the spectrum and render all landscpes drab
        and grey. So jump in your transporter and with the help of your
        faithful servant Catelite, restore Wizworld to its former glory.

        The landscapes in Wizworld are comprised of three colours each.
        Your task is to restore the original colours by shooting the Red,
        Green and Blue colour bubbles and then using Cat to collect the
        droplets of colour as they fall to the ground. Droplets collected
        will be stored in the cauldron displayed at the bottom of the
        screen, until such time as you have enough of each colour to make
        the target colour displayed in the cauldron on the far bottom
        right of the screen.

        In the three levels which have aliens on: one has red, one has
        green and one has blue. It is therefore necessary to move between
        the three levels using the tunnels to collect all three colours.
        To complete a level, you must colour in the whole landscape.
        After each colour is collected there is a bonus stage. After
        this, Wiz enters Wiz-Lab and is given a Wiz-Perk by his guardian
        angel. You may select one weapon or control, which will be
        magically endowed up on all subsequent Wizballs from birth, or
        opt for the bonus of 1000 points x Wiz-Level number.

        When certain aliens are killed they will deposit a green pearl
        which will remain stationary on the screen. If Wizball passes
        over this pearl and picks it up the first icon on the top of the
        screen will glow; this indicates that Wiz has the option to
        select a feature representated on the icon. If you want to select
        another feature collect more pearls until the icon you want is
        glowing. Several icons have two possible functions. The icons
        are, from left to right:

         1) Thrust     - Gives Wiz more control over the Wizball and
                         allows him to move it left or right.
            Antigrav   - Gives Wiz total control over the Wizball and
                         stops the bouncing.
         2) Beam       - Gives Wiz supa-beam weapon.
            Double     - Gives Wiz and cat automatic two-directional
                         firepower.
         3) Catellite  - Gives Wiz a cat fresh from training college.
         4) Blazers    - Gives Wiz and Cat super rapid fire blazers.
         5) Wiz Spray  - Gives Wiz mega spray protection.
            Cat Spray  - Does the same for Cat (but Wiz and Cat cannot
                         have spray at the same time).
         6) Smart Bomb - Kills every sprite in sight.
         7) Shields    - Gives Wiz and Cat shields for a limited period
                         only.

It's worth noting that despite these purporting to be the Spectrum instructions,
the Spectrum version of the game doesn't even have the bonus stage of which it
speaks. Tsk! Also, the "1000 points x Wiz-Level number" figure actually works
out to (10000 - (1000 x level number)) points. i.e. 9000 points on the first
world, 8000 points on the second world, 7000 on the third, and so on.

The instructions also neglect to point out that the Double weapon is utter crap
and should be avoided at all costs.



                ---------------- Control Freak ----------------

Default controls for Wizball are cursors for movement, Z to fire and X to select
the highlighted bonus icon. Default controls for Catellite are WADS for movement
and P to fire. Obviously these two sets of keys are somewhat mutually exclusive
and shouldn't be used simultaneously unless you're a champion at Travel Twister
(which is a fictional version of Twister I invented for car journeys in which
you use fingers instead of limbs - how it doesn't actually exist I'll never
know...).

Controls can be redefined to either keyboard or gamepads.



            ---------------- Ways To Break The Game ----------------

You can also change the colour depth and window settings in the game, although
the new settings will not take effect until you restart the game. You can
manually alter the settings in the file "config.txt" if you like by using values
of "16", "32" for the colour depth and "TRUE" or "FALSE" for the windowing.
The default setting is 16-bit windowed.

Obviously you can alter the volume of the sound and music as well.



  ---------------- How This Unfortunate Situation Came About ----------------

This version of Wizball was started in early 2006 as I'd promised Smila that I'd
remake it. Of course it's now 20 months later and I regret that decision
hugely. ;)

On the bright side, as with the making of Exolon DX, this has helped iron TONS
of bugs out of Retrengine, my 2D engine. Quite frankly I'm amazed that Exolon DX
even runs, given some of the gremlins I hoovered out of the codebase in the last
one and a half years.



          ---------------- Credit Where Credit's Due ----------------

Wizball was originally designed by Jon Hare and Chris Yates. It was programmed
by Chris Yates with graphics by Jon Hare. The original music was by Martin
Galway. The original cover illustration was by Bob Wakelin.

The remake was programmed by Graham Goring, with graphics by Trevor "Smila"
Storey and music by Infamous.

The Macintosh port is being handled by Peter Hull, and Scott Wightman is
handling the Linux port.

Retrengine makes use of the brilliant Allegro library, along with AllegroGL for
the shiny OpenGL bits and FMOD for the plippy sound bits.



                    ---------------- Thanks ----------------

Trevor Storey for... Oh, wait. He's meant to be on the blame list... ;)

Chris Nunn for jumping at the chance to do the music. I'm sure he regrets that
now, given how many tracks the game contains.

Peter Hull for being a reliable porting workhorse as always.

Scott Wightman for volunteering for to do the Linux port.

Peter Hanratty for being easily the best beta tester I had on this project. He
really sunk a lot of hours into Wizball and it helped immeasurably.

Muttley for giving useful feedback on joypad input and helping quash a lot of
bugs related to it.

Richard Jordan for his reverse psychology in telling me to shitcan the game all
the time. It didn't work! Hahaha! Crap, where did the last 20 months go?

Neil Walker for being a good egg. He didn't actually contribute to Wizball and
he only remakes crap games, but he's a splendid fellow and the glue which binds
Retrospec together. Not sure it's worth the effort these days, though.

Tomaz Kac for being lovely. Sure, he's done sod all since Head Over Heels and
Push Push Penguin but they're enough to let him coast for a good few years.

Marticus for running the Wizball Shrine and answering many questions about how
the C64 version of Wizball worked.

Carleton Handley for being one of those sods who moaned about the joystick
waggling. Honestly, you'd think he had arthritus or something.

Tim W for his unconditional support. I swear I could make a game about
molesting orphans and he'd be nice about it. Hmm... *rubs chin*

Oddbob for the nice news posts about the game and being a splendid fellow.

Paul Pridham, so that he doesn't moan at me. ;)

All the people at Retro Remakes, except for the ones I don't like. Although
*HE* doesn't really post there any more.

I'd also thank the peeps at the TIGforums, but as this is a remake I bet half
of them will never see it. Cor lummy, if it hasn't got abstract or pixel
graphics they just don't wanna' know... ;)



                 ---------------- Known Issues ----------------

None, thanks to the crack team of beta testers. Unless I forgot to fix something
that they reported, which is fairly likely...



               ---------------- Version History ----------------

v0.30 - First demo. Not much in there.
v0.70 - Second demo. Everything but the menus. And some other stuff.
v0.90 - Everything but the sound and completion effect.
v0.91 - As above, but with far less bugs and every game being recorded to track
 down further bugs. Also contains placeholder sound code for everything.
v0.95 - Finished other than missing one piece of music and another piece of
 music needing alteration. It should be relatively bug-free, but as you see I've
 allowed a whole 0.05 of versions for the bugs which my crack team of beta
 testers will find. ;)
v0.96 - Some bugs that the lovely testers found making it possible to use the
 joypad to define your keys properly, fixing a couple of graphical glitches,
 made the fuzz enemies pause properly (d'oh!) and fixed a few collision issues
 for Catellite.
v1.00 - Added final music tracks from Infamous and added music test mode to the
 option menu so you could listen to any tracks you unlock at your leisure.



                  ---------------- Legal Guff ----------------

Wizball code, graphics & music (c) 1987 Sensible Software.
Wizball as released by Retrospec & Smila, all programming (c) 2007 Retrospec.
Graphics (c) 2007 Trevor Storey.
Music arrangements & original themes (c) 2007 Infamous.



               ---------------- Contact and Website Details ----------------

Graham Goring - graham@duketastrophy.demon.co.uk
 - http://www.duketastrophy.demon.co.uk & http://www.drderekdoctors.com

Trevor Storey - http://www.merseyremakes.co.uk/

Infamous - http://www.infamousuk.com/

Retrospec - http://retrospec.sgn.net/

Retro Remakes - http://www.retroremakes.com/

Allegro - http://www.allegro.cc

AllegroGL - http://allegrogl.sourceforge.net/

fmod - http://www.fmod.org/

The Wizball Shrine - http://marticus.midnightrealm.org/wiz/

