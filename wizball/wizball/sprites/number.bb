Graphics 800,600,16,2
SetBuffer BackBuffer()

level_number = 6

filename$ = "level_"+level_number+"_tiles_new[set][16][16][0][0].bmp"

gfx = LoadImage (filename$)

a$ = ""
b$ = "// Restore!"+Chr$(13)+Chr$(10)

counter = 0
Dim draw_over_start(120)
Dim draw_over_end(120)

While KeyDown(1) = 0
	Cls

	DrawImage gfx,0,0
	mx = MouseX()
	my = MouseY()

	tilex = mx/16
	tiley = my/16
	tilenumber = tiley*32 + tilex

	Text 0,512,tilenumber

	If (MouseHit(1))
		replace_block_start = tilenumber
	EndIf

	If (MouseDown(1))
		replace_block_end = tilenumber
	EndIf

	If (MouseHit(2))
		with_block_start = tilenumber
		with_block_end = with_block_start + (replace_block_end - replace_block_start)

		a$ = a$ + "#DATA "+replace_block_start+","+replace_block_end+","+with_block_start+Chr$(13)+Chr$(10)
		b$ = b$ + "#DATA "+with_block_start+","+with_block_end+","+replace_block_start+Chr$(13)+Chr$(10)

		draw_over_start(counter) = replace_block_start
		draw_over_end(counter) = replace_block_end
		draw_over_start(counter+1) = with_block_start
		draw_over_end(counter+1) = with_block_end

		counter = counter + 2
	EndIf

	If (MouseHit(3))
		a$ = a$ + "// Gap!"+Chr$(13)+Chr$(10)
	EndIf

	For t=0 To counter-1
		tile_start_x = (draw_over_start(t) Mod 32) * 16
		tile_start_y = (draw_over_start(t) / 32) * 16

		tile_end_x = ((draw_over_end(t) Mod 32) * 16) + 16
		tile_end_y = ((draw_over_end(t) / 32) * 16) + 16

		Color 0,128,0
		Rect tile_start_x,tile_start_y,tile_end_x-tile_start_x,tile_end_y-tile_start_y
	Next

	tile_start_x = (replace_block_start Mod 32) * 16
	tile_start_y = (replace_block_start / 32) * 16

	tile_end_x = ((replace_block_end Mod 32) * 16) + 16
	tile_end_y = ((replace_block_end / 32) * 16) + 16

	Color 255,255,255
	Rect tile_start_x,tile_start_y,tile_end_x-tile_start_x,tile_end_y-tile_start_y,0

	Flip
Wend

file_id = WriteFile ("output.txt")

write_word_to_file(file_id,a$)

CloseFile(file_id)


EndGraphics

End





Function write_word_to_file(file_id,a$)

	For t=0 To Len(a$)-1
		char$ = Mid$(a$,t+1,1)
		byte = Asc(char$)
		WriteByte (file_id,byte)
	Next

End Function