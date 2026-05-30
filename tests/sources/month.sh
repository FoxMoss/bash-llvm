#!/bin/bash

shopt -s nocasematch

echo "Enter name of a month"
month="june"
real="june"

case $month in

  February | $real )
    echo "28/29 days in $month"
    ;;

  April | September | November)
    echo "30 days in $month"
    ;;

  January | March | May | July | August | October | December)
    echo "31 days in $month"
    ;;

  *)
    echo "Unknown month: $month"
    ;;
esac

star="*"
month="april"

case $month in

  February | $real )
    echo "28/29 days in $month"
    ;;

  September | November)
    echo "30 days in $month"
    ;;

  January | March | May | July | August | October | December | $star)
    echo "31 days in $month"
    ;;

  *)
    echo "Unknown month: $month"
    ;;
esac

month="*"

case $month in

  February | $real )
    echo "28/29 days in $month"
    ;;

  September | November)
    echo "30 days in $month"
    ;;

  January | March | May | July | August | October | December | $star)
    echo "31 days in $month"
    ;;

  *)
    echo "Unknown month: $month"
    ;;
esac

month="june"

case $month in

  February  )
    echo "28/29 days in $month"
    ;;

  September | November | ju* )
    echo "30 days in $month"
    ;;

  January | March | May | July | August | October | December )
    echo "31 days in $month"
    ;;

  *)
    echo "Unknown month: $month"
    ;;
esac

case $month in

  February  )
    echo "28/29 days in $month"
    ;;

  September | November | *une )
    echo "30 days in $month"
    ;;

  January | March | May | July | August | October | December )
    echo "31 days in $month"
    ;;

  *)
    echo "Unknown month: $month"
    ;;
esac

case $month in

  February  )
    echo "28/29 days in $month"
    ;;

  September | November | *un* )
    echo "30 days in $month"
    ;;

  January | March | May | July | August | October | December )
    echo "31 days in $month"
    ;;

  *)
    echo "Unknown month: $month"
    ;;
esac

case $month in

  February  )
    echo "28/29 days in $month"
    ;;

  September | November | *une* )
    echo "30 days in $month"
    ;;

  January | March | May | July | August | October | December )
    echo "31 days in $month"
    ;;

  *)
    echo "Unknown month: $month"
    ;;
esac
