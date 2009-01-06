(*****************************************************************************)
(*  libCUDF - CUDF (Common Upgrade Description Format) manipulation library  *)
(*  Copyright (C) 2009  Stefano Zacchiroli <zack@pps.jussieu.fr>             *)
(*                                                                           *)
(*  This program is free software: you can redistribute it and/or modify     *)
(*  it under the terms of the GNU General Public License as published by     *)
(*  the Free Software Foundation, either version 3 of the License, or (at    *)
(*  your option) any later version.                                          *)
(*                                                                           *)
(*  This program is distributed in the hope that it will be useful, but      *)
(*  WITHOUT ANY WARRANTY; without even the implied warranty of               *)
(*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *)
(*  General Public License for more details.                                 *)
(*                                                                           *)
(*  You should have received a copy of the GNU General Public License        *)
(*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    *)
(*****************************************************************************)

type solution

val solution : Cudf.universe -> solution

val satisfy_formula : Cudf.universe -> Cudf.vpkgformula -> bool
val disjoint : Cudf.universe -> Cudf.vpkglist -> bool

(** @return true if the given installation is consistent (in that case
    the returned string is pointless), false and if it is not (in that
    case the returned string is an explanation of why the installation
    is inconsistent) *)
val is_consistent : Cudf.universe -> bool * string

val is_solution : Cudf.universe -> solution -> bool * string
