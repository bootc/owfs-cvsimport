###########################################################
###########################################################
########## Enough of notebook, lets do Simulant! ##########
###########################################################
###########################################################

###########################################################
########## Command line parsing ###########################
###########################################################

proc CommandLineParsing { } {
    global argv
    global serve
    global dlist
    
    set port 0
    foreach a $argv {
        puts $a
        if { [regexp {^[0-9a-fA-F]{2}$} $a] == 1 } {
            lappend dlist $a
        } elseif { [regexp {[0-9]{3,}} $a] == 1 } {
            set serve(port) $a
        }
    }
    if { [llength $dlist] == 0 } {
        lappend dlist 10
    }
}
