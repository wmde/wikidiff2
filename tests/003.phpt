--TEST--
Diff test C: https://phabricator.wikimedia.org/T29993
--SKIPIF--
<?php if (!extension_loaded("wikidiff2")) print "skip"; ?>
--FILE--
<?php 
$x = <<<EOT
!!FUZZY!!Rajaa

EOT;

#---------------------------------------------------

$y = <<<EOT
Rajaa

EOT;

#---------------------------------------------------

print wikidiff2_do_diff( $x, $y, 2 );

?>
--EXPECT--
<tr>
  <td colspan="2" class="diff-lineno"><!--LINE 1--></td>
  <td colspan="2" class="diff-lineno"><!--LINE 1--></td>
</tr>
<tr>
  <td class="diff-marker">−</td>
  <td class="diff-deletedline"><div><del class="diffchange diffchange-inline">!!FUZZY!!</del>Rajaa</div></td>
  <td class="diff-marker">+</td>
  <td class="diff-addedline"><div>Rajaa</div></td>
</tr>
