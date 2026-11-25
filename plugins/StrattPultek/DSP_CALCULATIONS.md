# StrattPultek — 64 calculs mathématiques et physiques (v1.9.5)

Référentiel de conception et de vérification. Les équations servent à dimensionner ou contrôler la chaîne ; elles ne signifient pas que chaque opération est exécutée à chaque échantillon. Les calculs coûteux restent au taux de contrôle ou à prepare(), conformément au budget ARM MPC.

## Niveau, énergie et dynamique

1. Gain d'amplitude depuis les décibels : \(G=10^{L_{dB}/20}\).
2. Niveau depuis le gain : \(L_{dB}=20\log_{10}|G|\).
3. Rapport de puissance : \(P_2/P_1=10^{\Delta L_{dB}/10}\).
4. Rapport de tension à impédance égale : \(V_2/V_1=10^{\Delta L_{dB}/20}\).
5. Valeur efficace : \(x_{RMS}=\sqrt{N^{-1}\sum_{n=0}^{N-1}x_n^2}\).
6. Facteur de crête : \(C=|x|_{peak}/x_{RMS}\).
7. Marge avant écrêtage numérique : \(H=-20\log_{10}|x|_{peak}\) dBFS.
8. Énergie discrète : \(E=\sum_n x_n^2\).
9. Puissance moyenne : \(P=E/N\).
10. Corrélation stéréo normalisée : \(\rho=\frac{\sum LR}{\sqrt{\sum L^2\sum R^2}}\).

## Échantillonnage et filtres

11. Fréquence de Nyquist : \(f_N=f_s/2\).
12. Pulsation normalisée : \(\omega=2\pi f/f_s\).
13. Période d'échantillonnage : \(T_s=1/f_s\).
14. Coefficient d'un passe-bas 1er ordre : \(a=1-e^{-2\pi f_c/f_s}\).
15. Pôle discret associé : \(p=1-a=e^{-2\pi f_c/f_s}\).
16. Réponse du passe-bas : \(H(z)=a/(1-pz^{-1})\).
17. Réponse complexe à \(\omega\) : \(H(e^{j\omega})=a/(1-pe^{-j\omega})\).
18. Module du passe-bas : \(|H|=a/\sqrt{1+p^2-2p\cos\omega}\).
19. Retard de groupe : \(\tau_g(\omega)=-d\arg H(e^{j\omega})/d\omega\) échantillons.
20. Cascade de filtres : \(H_T(z)=\prod_i H_i(z)\).
21. Gains en cascade : \(L_T(f)=\sum_i20\log_{10}|H_i(f)|\).
22. Décalage d'octave : \(f_2/f_1=2\).
23. Décalage d'une demi-octave : \(f_2/f_1=\sqrt2\).
24. Fréquence géométrique : \(f_g=\sqrt{f_1f_2}\).
25. Coefficient biquad RBJ : \(\alpha=\sin(\omega_0)/(2Q)\).
26. Relation bande passante/Q : \(Q=f_0/(f_2-f_1)\) (approximation bande étroite).
27. Largeur en octaves : \(BW=\log_2(f_2/f_1)\).
28. Gain d'un filtre en décibels : \(A=10^{G_{dB}/40}\) pour les formules RBJ.
29. Réponse d'un biquad : \(H(z)=\frac{b_0+b_1z^{-1}+b_2z^{-2}}{1+a_1z^{-1}+a_2z^{-2}}\).
30. Stabilité d'un filtre IIR : pôles \(|p_i|<1\).

## Commandes, transitions et mélange

31. Constante de temps : \(\tau=1/(2\pi f_c)\).
32. Coefficient de lissage par bloc : \(\alpha=1-e^{-N/(\tau f_s)}\).
33. Lissage exponentiel : \(y_k=y_{k-1}+\alpha(x_k-y_{k-1})\).
34. Interpolation logarithmique : \(f(t)=e^{(1-t)\ln f_0+t\ln f_1}\).
35. Conversion d'un fader dB en gain : \(G_F=10^{F_{dB}/20}\).
36. Gain total en série : \(G_T=G_{trim}G_F\).
37. Mélange linéaire : \(y=x_d+(x_w-x_d)m\).
38. Mélange parallèle cohérent à somme unitaire : \(y=(1-m)x+mw\).
39. Gain de compensation du drive : \(G_D=1+kD\).
40. Compensation de niveau pré/post saturation : \(x_s=F(G_Dx)/G_D\).

## Non-linéarité, aliasing et robustesse

41. Courbe polynomiale C2 en 1.7.0 : \\(F(x)=x\\) si \\(|x|\\le1\\), \\(F(|x|)=1+t-t^3+t^4/2\\) pour \\(1<|x|<2\\) avec \\(t=|x|-1\\), puis plateau \\(1.5\\operatorname{sgn}(x)\\).
42. Asymétrie harmonique : \(F_a(x)=F(x)+\eta F(x)^2\).
43. Dérivée de la primitive : \(dI_F(x)/dx=F(x)\).
44. ADAA ordre 1 : \(y[n]=\frac{I_F(x[n])-I_F(x[n-1])}{x[n]-x[n-1]}\).
45. Limite ADAA quand \(x[n]\to x[n-1]\) : \(y[n]\to F(x[n])\).
46. Seuil relatif de stabilité numérique : \(|\Delta x|<\epsilon\max(1,|x_n|,|x_{n-1}|)\).
47. Densité de signal bornée : \(d[n]=\operatorname{clamp}(2|x[n]+x[n-1]|,0,1)\).
48. Mélange de coloration : \(g[n]=Grain\cdot Drive\cdot DriveMix\cdot d[n]\).
49. Filtre bloqueur DC : \(y[n]=x[n]-x[n-1]+p y[n-1]\), avec \(p=e^{-2\pi f_{DC}/f_s}\).
50. Vérification de crête de sortie : \(P_{out}=\max_n|G_T y[n]|\), puis \(H_{out}=-20\log_{10}(P_{out})\).

## Modèle fixe de coloration à lampe ajouté en 1.5.0, raffiné numériquement en 1.6.0 et lissé en 1.7.0

51. Gain fixe d'attaque du modèle en 1.7.0 : \\(u[n]=1.15x[n]\\).
52. Transfert C2 borné : pour \\(a=|x|\\), \\(F(x)=x\\) si \\(a\\le1\\), \\(F(x)=\\operatorname{sgn}(x)[1+t-t^3+t^4/2]\\) si \\(1<a<2\\), avec \\(t=a-1\\), et \\(F(x)=1.5\\operatorname{sgn}(x)\\) si \\(a\\ge2\\). Sa pente et sa courbure sont continues aux deux raccords. Transfert asymétrique : \\(F_a=F+0.045F^2\\).
53. Compensation de gain nominal : \\(s[n]=v[n]/1.15\\).
54. ADAA ordre 1 : \\(s_{ADAA}[n]=\\frac{I(u[n])-I(u[n-1])}{u[n]-u[n-1]}\\), avec limite différentielle lorsque les entrées sont presque égales.
55. Blocage DC indépendant par canal : \\(y[n]=s_{ADAA}[n]-s_{ADAA}[n-1]+e^{-2\\pi(3)/f_s}y[n-1]\\).
56. Position de la lampe dans le graphe : \\(y_{out}[n]=Fader(Valve(OutputTrim(Mix(EQDrive(x[n])))))\\).

## Calculs effectivement appliqués dans la version 1.5.0

- Seuil ADAA relatif au niveau interne (46), pour réduire l'annulation numérique dans la différence des primitives près des entrées voisines.
- Conversion dB/gain du fader (35), gain total de fin de chaîne (36), et lissage temporel (32–33).
- Conservation de la somme unitaire du Drive Mix et du Dry/Wet global (37–38) : éviter un gain artificiel lors du mélange de signaux corrélés.
- Vérification des réponses de filtres et des sélecteurs HF au taux d'échantillonnage réel (11–20), calculée en préparation ou au changement de bloc, pas par appel trigonométrique à chaque échantillon.

Les équations 1–50 sont un catalogue d'analyse et de validation ; les appliquer toutes comme des traitements supplémentaires dégraderait le son ou augmenterait inutilement la charge CPU.


## Raffinement numérique ADAA en 1.6.0 et transfert C2 en 1.7.0

Pour limiter l'annulation soustractive lorsque les deux échantillons sont proches, les primitives \(I_F\), \(I_{F^2}\), leur combinaison \(I_a=I_F+\eta I_{F^2}\), et le quotient de différences sont maintenant calculés en double précision. La limite différentielle est prise au milieu \(m=(x_n+x_{n-1})/2\) lorsque \(|\Delta x|\le10^{-7}\max(1,|x_n|,|x_{n-1}|)\), avec \(F(m)+\eta F(m)^2\). Le transfert et ses primitives analytiques restent identiques ; seul le calcul numérique du quotient est raffiné. Cela améliore la précision numérique, sans transformer ce modèle polynomial en simulation physique exacte d'une triode.


En 1.7.0, la primitive de F dans la zone de transition (t=|x|-1) est \\(I_F=1/2+t+t^2/2-t^4/4+t^5/10\\), et celle de F² est \\(I_{F^2}=1/3+t+t^2+t^3/3-t^4/2-t^5/5+t^6/6+t^7/7-t^8/8+t^9/36\\). Les raccords extérieurs sont obtenus par intégration exacte des branches linéaire et constante. Le choix du polynôme garantit \\(F'(1)=1, F''(1)=0, F'(2)=0, F''(2)=0\\), donc une continuité C2 du transfert. Les dérivées numériques des deux primitives ont été contrôlées sur les deux frontières et dans l'intervalle de transition. Le gain d'attaque du modèle est passé de 1.35 à 1.15 pour compenser la réduction de largeur de genou; l'asymétrie reste 0.045.


## Stabilisation locale du quotient ADAA (raffinement 1.7.1)

Pour un intervalle court entièrement contenu dans une branche polynomiale, la moyenne exacte de la fonction est évaluée par symétrie autour de \\(m=(x_n+x_{n-1})/2\\) : \\(F_a(m)+F_a''(m)\\Delta x^2/24+F_a^{(4)}(m)\\Delta x^4/1920+O(\\Delta x^6)\\). Dans la branche linéaire \\(F_a=x+\\eta x^2\\), cette expression est exacte après le terme quadratique. Dans le genou, le terme d'ordre quatre est inclus ; le polynôme \\(F_a\\) est de degré huit, donc le reste est d'ordre six. Le seuil relatif est \\(10^{-4}\\max(1,|x_n|,|x_{n-1}|)\\). Si l'intervalle touche ou traverse une frontière de branche, le calcul revient à la différence des primitives analytiques, sans approximation locale. Cette branche locale réduit l'annulation soustractive sans augmenter le nombre d'échantillons ni introduire de fonction transcendante dans la boucle audio.


## Filtres HP/LP ajoutés en 1.9.0

57. Pré-guerre bilinéaire préwarpé : \\(K=\\tan(\\pi f_c/f_s)\\), \\(a_1=(K-1)/(K+1)\\).
58. Passe-bas : \\(b_0=b_1=K/(1+K)\\), \\(y[n]=b_0x[n]+b_1x[n-1]-a_1y[n-1]\\).
59. Passe-haut : \\(b_0=1/(1+K), b_1=-b_0\\), même dénominateur; la réponse est à -3.01 dB à \\(f_c\\) et tend vers 6 dB/octave.

Les fréquences sont bornées à \\(0.49f_s\\), les contrôles sont lissés en domaine logarithmique et les coefficients sont recalculés par bloc. Le modèle est une réalisation bilinéaire stable du passe-haut/passe-bas 1er ordre documenté par Brainworx; ce n'est pas une reproduction du traitement Anti-Crush propriétaire.


## Numerical boundary protection added in 1.9.1

60. Finite host input bound: \(x_s=\operatorname{clamp}(x,-16,+16)\), retaining 24 dB of amplitude headroom above unity before recursive and polynomial DSP.
61. Output containment: \(y_s=y\) when \(y\) is finite; otherwise emit \(0\) and reset only the affected channel's recursive state.

These are defensive bounds for malformed or pathological host values, not a normal programme limiter and not a change to the intended EQ or saturation curves.


## ADAA initial-state condition added in 1.9.2

62. First-sample ADAA initialization: after a reset, let the first valid nonlinear input be \\(x_0\\). Set \\(x_{-1}=x_0\\), so the divided-difference limit is \\(y_0=F_a(x_0)\\), rather than the artificial startup average \\([I_a(x_0)-I_a(0)]/x_0\\). Subsequent samples use the ordinary first-order ADAA interval \\([x_{n-1},x_n]\\). The drive and fixed valve stages keep independent per-channel validity flags; reset and numerical recovery clear them. This is an initialization correction, not a change to the nonlinear transfer or steady-state frequency response.


## ADAA boundary conditioning added in 1.9.3

63. For a short interval crossing a polynomial boundary, split the integration interval at each knee \(b\in\{-2,-1,1,2\}\), then integrate each polynomial segment with five-point Gauss-Legendre quadrature:
\[
\int_a^b F_a(x)\,dx \approx \frac{b-a}{2}
\sum_{i=1}^{5}w_i F_a\!\left(\frac{a+b}{2}+\frac{b-a}{2}x_i\right).
\]
The five-point rule is exact for polynomials through degree nine; the asymmetric transfer \(F_a=F+\eta F^2\) is degree eight on each segment. Dividing the summed integral by the original interval width yields the ADAA interval average without subtracting nearly equal global primitive values. This path is limited to intervals below the existing relative \(10^{-4}\) threshold that cross a branch boundary; the local Taylor and ordinary closed-form paths are unchanged.


## Protection des paramètres hôte ajoutée en 1.9.4

64. Lecture bornée d'un paramètre : \(p_s=\operatorname{clamp}(p,p_{min},p_{max})\) si \(p\) est fini, sinon \(p_s=p_{défaut}\). Les paramètres de choix sont bornés avant conversion entière. Cette protection empêche qu'une automation VST malformée (NaN/Inf) ne contamine les lisseurs ou les indices de sélection. Elle est exécutée au taux de contrôle, jamais dans une boucle supplémentaire par échantillon.


## Sortie maître neutre ajoutée en 1.9.5

65. Gain maître sans interaction non linéaire : \(y[n]=G_{Output}G_{Fader}T(x_{EQ}[n])\), où \(T\) est l'étage de coloration fixe. Le gain Output est placé après \(T\) : modifier ou réinitialiser le paramètre 0 ne change donc ni le point de fonctionnement, ni les harmoniques, ni la saturation de cet étage. Le Fader dédié reste la dernière multiplication.
