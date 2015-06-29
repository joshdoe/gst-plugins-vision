
.function extractcolor_orc_copy32_0
.flags 2d
.dest 1 d guint8
.source 4 s guint8
.temp 2 t
select0lw t, s
select0wb d, t


.function extractcolor_orc_copy32_1
.flags 2d
.dest 1 d guint8
.source 4 s guint8
.temp 2 t
select0lw t, s
select1wb d, t


.function extractcolor_orc_copy32_2
.flags 2d
.dest 1 d guint8
.source 4 s guint8
.temp 2 t
select1lw t, s
select0wb d, t


.function extractcolor_orc_copy32_3
.flags 2d
.dest 1 d guint8
.source 4 s guint8
.temp 2 t
select1lw t, s
select1wb d, t


.function extractcolor_orc_copy64_0
.flags 2d
.dest 2 d guint16
.source 8 s guint16
.temp 4 t
select0ql t, s
select0lw d, t


.function extractcolor_orc_copy64_1
.flags 2d
.dest 2 d guint16
.source 8 s guint16
.temp 4 t
select0ql t, s
select1lw d, t


.function extractcolor_orc_copy64_2
.flags 2d
.dest 2 d guint16
.source 8 s guint16
.temp 4 t
select1ql t, s
select0lw d, t


.function extractcolor_orc_copy64_3
.flags 2d
.dest 2 d guint16
.source 8 s guint16
.temp 4 t
select1ql t, s
select1lw d, t